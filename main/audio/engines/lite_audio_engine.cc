/*
 * 备用的轻量音频引擎：直接抽取单声道 PCM 并可接入独立 WakeNet。
 * 此路径不提供设备端 AEC，不能用于替代 VoCat 播放期间持续监听所需的回声消除。
 * 唤醒对象和输出缓存分别加锁，模型资源可在不使用时释放并按需恢复。
 */

#include "lite_audio_engine.h"

#include <esp_log.h>
#include <esp_wn_models.h>

#include "wake_words/esp_wake_word.h"

#define TAG "LiteAudioEngine"

LiteAudioEngine::LiteAudioEngine() = default;
LiteAudioEngine::~LiteAudioEngine() = default;

// 以 16 kHz 和帧时长计算输出块大小，按模型可用性准备独立唤醒检测器。
bool LiteAudioEngine::Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) {
    codec_ = codec;
    models_list_ = models_list;
    frame_samples_ = frame_duration_ms * 16000 / 1000;
    output_buffer_.reserve(frame_samples_);

    should_have_wake_word_ = models_list != nullptr &&
        esp_srmodel_filter(models_list, ESP_WN_PREFIX, nullptr) != nullptr;
#if CONFIG_USE_ESP_WAKE_WORD
    should_have_wake_word_ = should_have_wake_word_ || models_list == nullptr;
#endif
    {
        std::lock_guard<std::mutex> lock(wake_word_mutex_);
        if (!CreateWakeWordLocked()) {
            return false;
        }
    }

    ESP_LOGI(TAG, "Initialized, WakeNet: %s", wake_word_ ? "yes" : "no");
    return true;
}

// 先把输入交给可用的唤醒检测器，再按语音处理开关决定是否输出音频。
void LiteAudioEngine::Feed(std::vector<int16_t>&& data) {
    {
        std::lock_guard<std::mutex> lock(wake_word_mutex_);
        if (wake_word_enabled_ && wake_word_) {
            wake_word_->Feed(data);
        }
    }
    if (voice_processing_enabled_) {
        OutputRawAudio(data);
    }
}

void LiteAudioEngine::EnableWakeWordDetection(bool enable) {
    std::lock_guard<std::mutex> lock(wake_word_mutex_);
    if (!wake_word_) {
        wake_word_enabled_ = false;
        return;
    }

    wake_word_enabled_ = enable;
    if (enable) {
        wake_word_->Start();
    } else {
        wake_word_->Stop();
    }
}

void LiteAudioEngine::EnableVoiceProcessing(bool enable) {
    voice_processing_enabled_ = enable;
    if (!enable) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_buffer_.clear();
    }
}

// 此实现没有 AEC，启用请求只产生提示；需要 AEC 时应选择支持回采的 AFE 路径。
void LiteAudioEngine::EnableDeviceAec(bool enable) {
    if (enable) {
        ESP_LOGW(TAG, "Device AEC is not supported by the lite engine");
    }
}

bool LiteAudioEngine::HasWakeWord() const {
    std::lock_guard<std::mutex> lock(wake_word_mutex_);
    return wake_word_ != nullptr;
}

bool LiteAudioEngine::IsWakeWordDetectionEnabled() const {
    return wake_word_enabled_;
}

bool LiteAudioEngine::IsVoiceProcessingEnabled() const {
    return voice_processing_enabled_;
}

size_t LiteAudioEngine::GetFeedSize() const {
    std::lock_guard<std::mutex> lock(wake_word_mutex_);
    if (wake_word_) {
        return wake_word_->GetFeedSize();
    }
    return frame_samples_;
}

void LiteAudioEngine::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = std::move(callback);
}

void LiteAudioEngine::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = std::move(callback);
}

void LiteAudioEngine::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = std::move(callback);
}

void LiteAudioEngine::EncodeWakeWordData() {
    std::lock_guard<std::mutex> lock(wake_word_mutex_);
    if (wake_word_) {
        wake_word_->EncodeWakeWordData();
    }
}

bool LiteAudioEngine::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    std::lock_guard<std::mutex> lock(wake_word_mutex_);
    return wake_word_ && wake_word_->GetWakeWordOpus(opus);
}

const std::string& LiteAudioEngine::GetLastDetectedWakeWord() const {
    return wake_word_ ? wake_word_->GetLastDetectedWakeWord() : empty_wake_word_;
}

// 持有唤醒对象锁后销毁模型实例，避免正在 Feed 时释放检测器。
void LiteAudioEngine::ReleaseWakeWordResources() {
    std::lock_guard<std::mutex> lock(wake_word_mutex_);
    wake_word_enabled_ = false;
    if (wake_word_) {
        wake_word_->Stop();
        wake_word_.reset();
        ESP_LOGI(TAG, "Released standalone WakeNet resources");
    }
}

bool LiteAudioEngine::RestoreWakeWordResources() {
    std::lock_guard<std::mutex> lock(wake_word_mutex_);
    return CreateWakeWordLocked();
}

// 调用方必须已持有 wake_word_mutex_；重新创建检测器并绑定原有业务回调。
bool LiteAudioEngine::CreateWakeWordLocked() {
    if (!should_have_wake_word_) {
        return true;
    }
    if (wake_word_) {
        return true;
    }

    auto wake_word = std::make_unique<EspWakeWord>();
    wake_word->OnWakeWordDetected([this](const std::string& detected_wake_word) {
        wake_word_enabled_ = false;
        if (wake_word_detected_callback_) {
            wake_word_detected_callback_(detected_wake_word);
        }
    });
    if (!wake_word->Initialize(codec_, models_list_)) {
        ESP_LOGE(TAG, "Failed to initialize standalone WakeNet");
        return false;
    }
    wake_word_ = std::move(wake_word);
    return true;
}

// 交错多通道只取首通道，凑足 frame_samples_ 后移动交出一帧 PCM。
void LiteAudioEngine::OutputRawAudio(const std::vector<int16_t>& data) {
    if (!output_callback_ || codec_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(output_mutex_);
    const size_t channels = codec_->input_channels();
    if (channels <= 1) {
        output_buffer_.insert(output_buffer_.end(), data.begin(), data.end());
    } else {
        for (size_t i = 0; i < data.size(); i += channels) {
            output_buffer_.push_back(data[i]);
        }
    }

    while (output_buffer_.size() >= static_cast<size_t>(frame_samples_)) {
        if (output_buffer_.size() == static_cast<size_t>(frame_samples_)) {
            output_callback_(std::move(output_buffer_));
            output_buffer_.clear();
            output_buffer_.reserve(frame_samples_);
        } else {
            output_callback_(std::vector<int16_t>(
                output_buffer_.begin(), output_buffer_.begin() + frame_samples_));
            output_buffer_.erase(output_buffer_.begin(), output_buffer_.begin() + frame_samples_);
        }
    }
}

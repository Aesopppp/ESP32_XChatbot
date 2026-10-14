/*
 * 独立 WakeNet 检测器，供轻量音频引擎等备用路径使用。
 * 多通道输入取首通道，按模型要求的块大小检测；命中后停止本轮检测并回调。
 * 此实现不编码唤醒音频；当前 VoCat 的 AFE 路径由 AfeAudioEngine 管理。
 */

#include "esp_wake_word.h"
#include <esp_log.h>


#define TAG "EspWakeWord"

EspWakeWord::EspWakeWord() {
}

EspWakeWord::~EspWakeWord() {
    if (wakenet_data_ != nullptr) {
        wakenet_iface_->destroy(wakenet_data_);
    }
    if (owns_models_ && wakenet_model_ != nullptr) {
        esp_srmodel_deinit(wakenet_model_);
    }
}

// 使用传入模型列表，或自行加载 model 分区；只在自行加载时承担对应释放责任。
bool EspWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    codec_ = codec;

    if (models_list == nullptr) {
        wakenet_model_ = esp_srmodel_init("model");
        owns_models_ = wakenet_model_ != nullptr;
    } else {
        wakenet_model_ = models_list;
    }

    if (wakenet_model_ == nullptr || wakenet_model_->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }
    if (wakenet_model_->num == 0) {
        ESP_LOGE(TAG, "No model found");
        return false;
    }
    char *model_name = esp_srmodel_filter(wakenet_model_, ESP_WN_PREFIX, nullptr);
    if (model_name == nullptr) {
        ESP_LOGE(TAG, "No WakeNet model found");
        return false;
    }
    wakenet_iface_ = (esp_wn_iface_t*)esp_wn_handle_from_name(model_name);
    if (wakenet_iface_ == nullptr) {
        ESP_LOGE(TAG, "No WakeNet interface found for %s", model_name);
        return false;
    }
    wakenet_data_ = wakenet_iface_->create(model_name, DET_MODE_95);
    if (wakenet_data_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create WakeNet model %s", model_name);
        return false;
    }

    int frequency = wakenet_iface_->get_samp_rate(wakenet_data_);
    int audio_chunksize = wakenet_iface_->get_samp_chunksize(wakenet_data_);
    ESP_LOGI(TAG, "Wake word(%s),freq: %d, chunksize: %d", model_name, frequency, audio_chunksize);

    return true;
}

void EspWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback;
}

void EspWakeWord::Start() {
    running_ = true;
}

void EspWakeWord::Stop() {
    running_ = false;

    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    input_buffer_.clear();
}

// 在输入锁内检查运行状态，累积到模型块大小后检测；命中则保存词名、停检测并回调。
void EspWakeWord::Feed(const std::vector<int16_t>& data) {
    if (wakenet_data_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    // Check running state inside lock to avoid TOCTOU race with Stop()
    if (!running_) {
        return;
    }

    if (codec_->input_channels() > 1) {
        for (size_t i = 0; i < data.size(); i += codec_->input_channels()) {
            input_buffer_.push_back(data[i]);
        }
    } else {
        input_buffer_.insert(input_buffer_.end(), data.begin(), data.end());
    }

    int chunksize = wakenet_iface_->get_samp_chunksize(wakenet_data_);
    while (input_buffer_.size() >= chunksize) {
        int res = wakenet_iface_->detect(wakenet_data_, input_buffer_.data());
        if (res > 0) {
            last_detected_wake_word_ = wakenet_iface_->get_word_name(wakenet_data_, res);
            running_ = false;
            input_buffer_.clear();

            if (wake_word_detected_callback_) {
                wake_word_detected_callback_(last_detected_wake_word_);
            }
            break;
        }
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + chunksize);
    }
}

size_t EspWakeWord::GetFeedSize() {
    if (wakenet_data_ == nullptr) {
        return 0;
    }
    return wakenet_iface_->get_samp_chunksize(wakenet_data_);
}

// 独立 WakeNet 路径暂未实现唤醒音频编码，此接口保留为空。
void EspWakeWord::EncodeWakeWordData() {
}

// 没有唤醒 Opus 数据可取，调用方应处理 false 返回值。
bool EspWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    return false;
}

/*
 * 当前 ESP32-S3 的 AFE 音频处理路径：AEC、VAD 和本地唤醒。
 * Feed 提交麦克风与回采通道，ProcessingTask 获取处理结果，再输出定长 PCM 与检测事件。
 * 跨任务开关 / 清理请求延后到处理任务执行，generation 用于拒绝旧会话的迟到结果。
 */

#ifndef AFE_AUDIO_ENGINE_H
#define AFE_AUDIO_ENGINE_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <esp_afe_sr_models.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "audio_engine.h"
#include "wake_words/wake_word_audio_cache.h"

class CustomWakeWord;

class AfeAudioEngine : public AudioEngine {
public:
    AfeAudioEngine();
    ~AfeAudioEngine() override;

    bool Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) override;
    void Feed(std::vector<int16_t>&& data) override;

    void EnableWakeWordDetection(bool enable) override;
    void EnableVoiceProcessing(bool enable) override;
    void EnableDeviceAec(bool enable) override;

    bool HasWakeWord() const override;
    bool IsWakeWordDetectionEnabled() const override;
    bool IsVoiceProcessingEnabled() const override;
    bool IsAfeWakeWord() const override { return HasWakeWord(); }
    size_t GetFeedSize() const override;

    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) override;
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;

    void EncodeWakeWordData() override;
    bool GetWakeWordOpus(std::vector<uint8_t>& opus) override;
    const std::string& GetLastDetectedWakeWord() const override { return last_detected_wake_word_; }

private:
    // WakeNet 是固定唤醒词模型；MultiNet 路径用于配置的自定义词检测，二者资源需求不同。
    enum class WakeDetector {
        kNone,
        kWakeNet,
        kMultiNet,
    };

    static constexpr EventBits_t kWakeWordEnabled = 1 << 0;
    static constexpr EventBits_t kVoiceProcessingEnabled = 1 << 1;
    static constexpr EventBits_t kAfeActive = 1 << 2;
    static constexpr size_t kProcessingTaskStackSize = 4096;

    AudioCodec* codec_ = nullptr;
    srmodel_list_t* models_ = nullptr;
    bool owns_models_ = false;
    const esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
    EventGroupHandle_t event_group_ = nullptr;
    TaskHandle_t processing_task_ = nullptr;
    StaticTask_t* processing_task_buffer_ = nullptr;
    StackType_t* processing_task_stack_ = nullptr;
    int frame_samples_ = 0;
    bool is_speaking_ = false;
    std::atomic<bool> device_aec_enabled_{false};
    // Deferred AFE buffer reset, performed by ProcessingTask (see UpdateActiveState)
    // 只在控制线程置位，由 ProcessingTask 重置 AFE 内部缓冲，避免与 fetch 并发操作。
    std::atomic<bool> reset_pending_{false};
    // Deferred WakeNet/AEC toggles, applied by ProcessingTask (see ApplyAfeControls)
    // WakeNet / AEC 控制变更延后到音频处理线程应用。
    std::atomic<bool> afe_control_dirty_{false};
    // Deferred output_buffer_ clear, performed by the output-producing task
    // 输出 PCM 的拼帧缓冲由产生输出的任务清理。
    std::atomic<bool> output_reset_pending_{false};
    // Incremented whenever an active AFE session is invalidated. ProcessingTask
    // uses it to reject a fetch result produced before a disable/re-enable cycle.
    // 每次使旧处理会话失效时递增，处理任务据此丢弃开关切换前产生的结果。
    std::atomic<uint32_t> control_generation_{0};
    WakeDetector wake_detector_ = WakeDetector::kNone;

    std::unique_ptr<CustomWakeWord> custom_wake_word_;
    std::vector<std::string> wake_words_;
    std::string last_detected_wake_word_;
    std::vector<int16_t> input_buffer_;
    std::vector<int16_t> output_buffer_;
    std::mutex input_buffer_mutex_;

    std::function<void(const std::string&)> wake_word_detected_callback_;
    std::function<void(std::vector<int16_t>&&)> output_callback_;
    std::function<void(bool)> vad_state_change_callback_;

    TaskHandle_t wake_word_encode_task_ = nullptr;
    StaticTask_t* wake_word_encode_task_buffer_ = nullptr;
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    // 最近的唤醒 PCM 与其 Opus 编码结果分别保存；不会持续累积完整对话。
    WakeWordAudioCache wake_word_audio_cache_;
    std::deque<std::vector<uint8_t>> wake_word_opus_;
    std::mutex wake_word_mutex_;
    std::condition_variable wake_word_cv_;

    void ProcessingTask();
    void UpdateActiveState();
    void UpdateAecState();
    void ApplyAfeControls();
    void ApplyPendingReset();
    void OutputRawAudio(const std::vector<int16_t>& data);
    void HandleWakeWordResult(const afe_fetch_result_t* result);
    void HandleVoiceResult(const afe_fetch_result_t* result);
};

#endif

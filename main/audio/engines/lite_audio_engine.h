/*
 * 备用的轻量音频引擎：直接抽取单声道 PCM 并可接入独立 WakeNet。
 * 此路径不提供设备端 AEC，不能用于替代 VoCat 播放期间持续监听所需的回声消除。
 * 唤醒对象和输出缓存分别加锁，模型资源可在不使用时释放并按需恢复。
 */

#ifndef LITE_AUDIO_ENGINE_H
#define LITE_AUDIO_ENGINE_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio_engine.h"

class EspWakeWord;

class LiteAudioEngine : public AudioEngine {
public:
    LiteAudioEngine();
    ~LiteAudioEngine() override;

    bool Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) override;
    void Feed(std::vector<int16_t>&& data) override;

    void EnableWakeWordDetection(bool enable) override;
    void EnableVoiceProcessing(bool enable) override;
    void EnableDeviceAec(bool enable) override;

    bool HasWakeWord() const override;
    bool IsWakeWordDetectionEnabled() const override;
    bool IsVoiceProcessingEnabled() const override;
    bool IsAfeWakeWord() const override { return false; }
    size_t GetFeedSize() const override;

    // Release the standalone WakeNet allocation while it cannot be used, then
    // recreate it before wake word detection is enabled again.
    void ReleaseWakeWordResources();
    bool RestoreWakeWordResources();

    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) override;
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;

    void EncodeWakeWordData() override;
    bool GetWakeWordOpus(std::vector<uint8_t>& opus) override;
    const std::string& GetLastDetectedWakeWord() const override;

private:
    AudioCodec* codec_ = nullptr;
    std::unique_ptr<EspWakeWord> wake_word_;
    srmodel_list_t* models_list_ = nullptr;
    bool should_have_wake_word_ = false;
    std::atomic<bool> wake_word_enabled_ = false;
    std::atomic<bool> voice_processing_enabled_ = false;
    int frame_samples_ = 0;
    std::vector<int16_t> output_buffer_;
    // 保护拼帧缓冲，唤醒对象使用另一个锁，防止资源释放与检测并发。
    std::mutex output_mutex_;
    mutable std::mutex wake_word_mutex_;

    std::function<void(const std::string&)> wake_word_detected_callback_;
    std::function<void(std::vector<int16_t>&&)> output_callback_;
    std::function<void(bool)> vad_state_change_callback_;
    std::string empty_wake_word_;

    bool CreateWakeWordLocked();
    void OutputRawAudio(const std::vector<int16_t>& data);
};

#endif

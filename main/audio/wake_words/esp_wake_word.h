/*
 * 独立 WakeNet 检测器，供轻量音频引擎等备用路径使用。
 * 多通道输入取首通道，按模型要求的块大小检测；命中后停止本轮检测并回调。
 * 此实现不编码唤醒音频；当前 VoCat 的 AFE 路径由 AfeAudioEngine 管理。
 */

#ifndef ESP_WAKE_WORD_H
#define ESP_WAKE_WORD_H

#include <esp_wn_iface.h>
#include <esp_wn_models.h>
#include <model_path.h>

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

#include "audio_codec.h"
#include "wake_word.h"

class EspWakeWord : public WakeWord {
public:
    EspWakeWord();
    ~EspWakeWord();

    bool Initialize(AudioCodec* codec, srmodel_list_t* models_list);
    void Feed(const std::vector<int16_t>& data);
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback);
    void Start();
    void Stop();
    size_t GetFeedSize();
    void EncodeWakeWordData();
    bool GetWakeWordOpus(std::vector<uint8_t>& opus);
    const std::string& GetLastDetectedWakeWord() const { return last_detected_wake_word_; }

private:
    esp_wn_iface_t *wakenet_iface_ = nullptr;
    model_iface_data_t *wakenet_data_ = nullptr;
    srmodel_list_t *wakenet_model_ = nullptr;
    bool owns_models_ = false;
    AudioCodec* codec_ = nullptr;
    std::atomic<bool> running_ = false;

    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    std::string last_detected_wake_word_;
    std::vector<int16_t> input_buffer_;
    std::mutex input_buffer_mutex_;
};

#endif

/*
 * MultiNet 自定义命令词检测器，可作为本地唤醒的一种可选软件实现。
 * 需要对应语言模型和命令表；当前 VoCat 固定的“你好小益”WakeNet 模型并不由本文件改名生成。
 * 支持交错原始 PCM 或单声道处理结果；可选回溯音频使用 PSRAM 缓存再单独编码。
 */

#ifndef CUSTOM_WAKE_WORD_H
#define CUSTOM_WAKE_WORD_H

#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_mn_iface.h>
#include <esp_mn_models.h>
#include <model_path.h>

#include <deque>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "audio_codec.h"
#include "wake_word.h"
#include "wake_word_audio_cache.h"

class CustomWakeWord : public WakeWord {
public:
    CustomWakeWord();
    ~CustomWakeWord();

    bool Initialize(AudioCodec* codec, srmodel_list_t* models_list);
    void Feed(const std::vector<int16_t>& data);
    void FeedMono(const int16_t* data, size_t samples);
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback);
    void Start();
    void Stop();
    size_t GetFeedSize();
    void EncodeWakeWordData();
    bool GetWakeWordOpus(std::vector<uint8_t>& opus);
    const std::string& GetLastDetectedWakeWord() const { return last_detected_wake_word_; }

private:
    // command 是模型识别串，text 是展示文字，action="wake" 才触发唤醒回调。
    struct Command {
        std::string command;
        std::string text;
        std::string action;
    };

    // multinet 相关成员变量
    esp_mn_iface_t* multinet_ = nullptr;
    model_iface_data_t* multinet_model_data_ = nullptr;
    srmodel_list_t *models_ = nullptr;
    bool owns_models_ = false;
    char* mn_name_ = nullptr;
    std::string language_ = "cn";
    int duration_ = 3000;
    float threshold_ = 0.2;
    std::deque<Command> commands_;
 
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    AudioCodec* codec_ = nullptr;
    std::string last_detected_wake_word_;
    std::atomic<bool> running_ = false;
    std::vector<int16_t> input_buffer_;
    // 保护检测拼块与 Stop 清理；回调在检测调用线程执行，不自动切换到应用主任务。
    std::mutex input_buffer_mutex_;

    TaskHandle_t wake_word_encode_task_ = nullptr;
    StaticTask_t* wake_word_encode_task_buffer_ = nullptr;
    StackType_t* wake_word_encode_task_stack_ = nullptr;
    WakeWordAudioCache wake_word_audio_cache_;
    std::deque<std::vector<uint8_t>> wake_word_opus_;
    std::mutex wake_word_mutex_;
    // 编码任务通知消费方，空 Opus 包是本轮结束标记。
    std::condition_variable wake_word_cv_;

    void FeedSamples(const int16_t* data, size_t samples, bool mono);
    void ParseWakenetModelConfig();
};

#endif

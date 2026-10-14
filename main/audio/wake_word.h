/*
 * 本地唤醒检测统一接口：模型从固件资源加载，在 ESP32 上对麦克风 PCM 运算。
 * 唤醒词能力来自 ESP-SR 软件和模型，并非开发板上的独立唤醒芯片。
 * 是否提供唤醒前音频编码取决于具体实现，不能假设每个检测器都支持音频回溯。
 */

#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <string>
#include <vector>
#include <functional>

#include <model_path.h>
#include "audio_codec.h"

class WakeWord {
public:
    virtual ~WakeWord() = default;
    
    virtual bool Initialize(AudioCodec* codec, srmodel_list_t* models_list) = 0;
    virtual void Feed(const std::vector<int16_t>& data) = 0;
    virtual void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual size_t GetFeedSize() = 0;
    virtual void EncodeWakeWordData() = 0;
    virtual bool GetWakeWordOpus(std::vector<uint8_t>& opus) = 0;
    virtual const std::string& GetLastDetectedWakeWord() const = 0;
};

#endif

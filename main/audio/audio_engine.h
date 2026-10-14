#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <model_path.h>

#include "audio_codec.h"

// 音频处理引擎接口：接收交错 PCM，输出可编码的单声道帧、VAD 和唤醒事件。
// AFE 与轻量引擎实现同一接口；板级通道数/回采能力由 AudioCodec 提供。
class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    virtual bool Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) = 0;
    // 输入数据通过移动转移所有权；回调所在线程取决于具体引擎实现。
    virtual void Feed(std::vector<int16_t>&& data) = 0;

    virtual void EnableWakeWordDetection(bool enable) = 0;
    virtual void EnableVoiceProcessing(bool enable) = 0;
    virtual void EnableDeviceAec(bool enable) = 0;

    virtual bool HasWakeWord() const = 0;
    virtual bool IsWakeWordDetectionEnabled() const = 0;
    virtual bool IsVoiceProcessingEnabled() const = 0;
    virtual bool IsAfeWakeWord() const = 0;
    // 返回处理器需要的每通道采样点数，采集层据此按实际输入通道数准备交错 PCM。
    virtual size_t GetFeedSize() const = 0;

    virtual void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) = 0;
    virtual void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) = 0;
    // speaking 表示本地语音活动；残余回声也可能触发，不能单凭它就取消 ACOS 回答。
    virtual void OnVadStateChange(std::function<void(bool speaking)> callback) = 0;

    virtual void EncodeWakeWordData() = 0;
    virtual bool GetWakeWordOpus(std::vector<uint8_t>& opus) = 0;
    virtual const std::string& GetLastDetectedWakeWord() const = 0;
};

#endif

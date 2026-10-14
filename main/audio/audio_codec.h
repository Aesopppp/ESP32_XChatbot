#ifndef _AUDIO_CODEC_H
#define _AUDIO_CODEC_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <driver/i2s_std.h>
#include <esp_idf_version.h>

#include <vector>
#include <string>
#include <functional>

#include "board.h"

// I2S DMA：6 个描述符，每个 240 帧。调整需兼顾延迟、断流容忍度及内部 DMA 内存。
#define AUDIO_CODEC_DMA_DESC_NUM 6
#define AUDIO_CODEC_DMA_FRAME_NUM 240

// ESP-IDF 6 removed i2s_port_t and changed i2s_chan_config_t::id to an integer.
// Keep numeric I2S controller IDs usable on targets where IDF 5 does not expose
// every value through the target-specific i2s_port_t enum (for example ESP32-C3).
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#define XIAOZHI_I2S_PORT(port) (port)
#else
#define XIAOZHI_I2S_PORT(port) static_cast<i2s_port_t>(port)
#endif

// 板级音频硬件抽象，负责 PCM I/O、音量和输入增益，不负责 Opus 或云端协议。
// Read/Write 的 samples 为 int16_t 元素数；输入多通道按采样时刻交错存放。
class AudioCodec {
public:
    AudioCodec();
    virtual ~AudioCodec();
    
    // 音量百分比写入 NVS 并由具体 codec 应用到硬件；0 表示静音。
    virtual void SetOutputVolume(int volume);
    virtual void SetInputGain(float gain);
    virtual void EnableInput(bool enable);
    virtual void EnableOutput(bool enable);

    virtual void OutputData(std::vector<int16_t>& data);
    virtual bool InputData(std::vector<int16_t>& data);
    // 通用实现恢复并校验音量；输入 / 输出开关由具体硬件和音频服务另行管理。
    // VoCat 的开机默认值在板级另有处理。
    virtual void Start();

    inline bool duplex() const { return duplex_; }
    inline bool input_reference() const { return input_reference_; }
    inline int input_sample_rate() const { return input_sample_rate_; }
    inline int output_sample_rate() const { return output_sample_rate_; }
    inline int input_channels() const { return input_channels_; }
    inline int output_channels() const { return output_channels_; }
    inline int output_volume() const { return output_volume_; }
    inline float input_gain() const { return input_gain_; }
    inline bool input_enabled() const { return input_enabled_; }
    inline bool output_enabled() const { return output_enabled_; }

protected:
    i2s_chan_handle_t tx_handle_ = nullptr;
    i2s_chan_handle_t rx_handle_ = nullptr;

    bool duplex_ = false;
    // 输入中是否包含扬声器回采参考，决定 AFE 的麦克风 / 参考通道格式。
    bool input_reference_ = false;
    bool input_enabled_ = false;
    bool output_enabled_ = false;
    int input_sample_rate_ = 0;
    int output_sample_rate_ = 0;
    int input_channels_ = 1;
    int output_channels_ = 1;
    // 成员初值不是设备最终开机值，Start 和 VoCat 的 NVS 策略会再次决定音量。
    int output_volume_ = 70;
    float input_gain_ = 0.0;

    virtual int Read(int16_t* dest, int samples) = 0;
    virtual int Write(const int16_t* data, int samples) = 0;
};

#endif // _AUDIO_CODEC_H

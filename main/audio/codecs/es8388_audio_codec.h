/*
 * ES8388 音频器件的备用 esp_codec_dev 适配。
 * 负责 I2C 控制、I2S PCM、输入 / 输出开关及硬件音量；不处理 Opus 或云端对话。
 * 只有板级选择此实现时才启用，不能因型号相近就套用当前 VoCat 的双器件驱动。
 */

#ifndef _ES8388_AUDIO_CODEC_H
#define _ES8388_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/i2c_master.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <mutex>


class Es8388AudioCodec : public AudioCodec {
private:
    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;
    const audio_codec_if_t* codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;
    bool output_device_opened_ = false;
    gpio_num_t pa_pin_ = GPIO_NUM_NC;
    std::mutex data_if_mutex_;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    Es8388AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        gpio_num_t pa_pin, uint8_t es8388_addr, bool input_reference = false);
    virtual ~Es8388AudioCodec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
};

#endif // _ES8388_AUDIO_CODEC_H

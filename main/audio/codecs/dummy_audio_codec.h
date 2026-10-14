/*
 * 无真实音频硬件的占位实现，保留 AudioCodec 接口便于兼容。
 * Read / Write 返回 0，不会采集麦克风或播放声音；不是当前 VoCat 的音频驱动。
 */

#ifndef _DUMMY_AUDIO_CODEC_H
#define _DUMMY_AUDIO_CODEC_H

#include "audio_codec.h"

class DummyAudioCodec : public AudioCodec {
private:
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    DummyAudioCodec(int input_sample_rate, int output_sample_rate);
    virtual ~DummyAudioCodec();
};

#endif // _DUMMY_AUDIO_CODEC_H
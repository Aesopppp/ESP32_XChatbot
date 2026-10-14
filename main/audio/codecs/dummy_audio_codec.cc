/*
 * 无真实音频硬件的占位实现，保留 AudioCodec 接口便于兼容。
 * Read / Write 返回 0，不会采集麦克风或播放声音；不是当前 VoCat 的音频驱动。
 */

#include "dummy_audio_codec.h"

DummyAudioCodec::DummyAudioCodec(int input_sample_rate, int output_sample_rate) {
    duplex_ = true;
    input_reference_ = false;
    input_channels_ = 1;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
}

DummyAudioCodec::~DummyAudioCodec() {
}

int DummyAudioCodec::Read(int16_t* dest, int samples) {
    return 0;
}

int DummyAudioCodec::Write(const int16_t* data, int samples) {
    return 0;
}

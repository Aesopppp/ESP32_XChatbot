/*
 * 本地唤醒音频环形缓存，使用 PSRAM 保存最近一段单声道 PCM。
 * 用于可选的唤醒词音频编码，不是 SD 卡录音，也不是完整对话文件。
 * 容量 / 偏移单位为采样点，读写位置由互斥锁保护。
 */

#ifndef WAKE_WORD_AUDIO_CACHE_H
#define WAKE_WORD_AUDIO_CACHE_H

#include <cstddef>
#include <cstdint>
#include <mutex>

class WakeWordAudioCache {
public:
    WakeWordAudioCache() = default;
    ~WakeWordAudioCache();

    WakeWordAudioCache(const WakeWordAudioCache&) = delete;
    WakeWordAudioCache& operator=(const WakeWordAudioCache&) = delete;

    bool Initialize(size_t sample_count);
    void Store(const int16_t* data, size_t samples);
    // offset 从缓存最旧采样点起算，返回实际读取点数；不会消费或删除缓存内容。
    size_t Read(size_t offset, int16_t* output, size_t samples) const;
    size_t Size() const;
    void Clear();

private:
    int16_t* buffer_ = nullptr;
    size_t capacity_ = 0;
    size_t size_ = 0;
    // 指向下一次写入的位置，满容量时新采样覆盖最旧采样。
    size_t write_position_ = 0;
    mutable std::mutex mutex_;
};

#endif

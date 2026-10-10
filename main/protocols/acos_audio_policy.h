#pragma once

#include <cstdint>

// 只决定何时释放已收到的 Opus 帧，不分配内存，也不阻塞麦克风和网络任务。
struct AcosPlaybackBuffer {
    bool buffering = true;
    bool delivered = false;
    int64_t waiting_since = 0;

    // queued_ms 是估算的压缩帧总时长，now 是单调时钟微秒。
    // 累积 300ms、回答已结束、等待满 600ms，任一满足便开始释放帧。
    // 等待上限避免短句或慢流一直不播，不保证坏网络下完全没有停顿。
    constexpr bool Ready(unsigned queued_ms, bool finished, int64_t now) {
        if (!buffering)
            return true;
        if (!waiting_since)
            waiting_since = now;
        if (queued_ms >= 300 || finished || now - waiting_since >= 600000) {
            buffering = false;
            return true;
        }
        return false;
    }
    // 已开始播放后发现下游完全空闲，才算欠载；重新进入预缓冲，而非结束回答。
    constexpr bool Underrun(bool idle) {
        if (!idle || !delivered || buffering)
            return false;
        buffering = true;
        waiting_since = 0;
        return true;
    }
};

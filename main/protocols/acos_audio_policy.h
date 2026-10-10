#pragma once

#include <cstdint>

// 只决定何时释放已收到的 Opus 帧，不分配内存，也不阻塞麦克风和网络任务。
struct AcosPlaybackBuffer {
    bool buffering = true;
    bool delivered = false;
    int64_t waiting_since = 0;

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
    constexpr bool Underrun(bool idle) {
        if (!idle || !delivered || buffering)
            return false;
        buffering = true;
        waiting_since = 0;
        return true;
    }
};

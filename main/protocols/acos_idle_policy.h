#pragma once

#include <cstdint>

// 连续对话的静默计时只在主任务更新，使用单调时钟微秒。
// 本地 VAD/音频包/心跳都不是确认的人声输入，不能无限延长会话。
struct AcosIdlePolicy {
    int64_t idle_since_us = -1;
    bool server_speaking = false;

    // 首次唤醒/重新打开会话时丢弃上一会话的语音状态。
    constexpr void Start(int64_t now) {
        server_speaking = false;
        BeginListening(now);
    }
    // 回答播完、手动打断、显式唤醒后重新给用户完整的等待时间。
    // 若云端已经确认用户正在说话，切换监听状态不能丢失这个保护。
    constexpr void BeginListening(int64_t now) { idle_since_us = now; }
    constexpr void SpeechStarted(int64_t now) {
        server_speaking = true;
        idle_since_us = now;
    }
    constexpr void SpeechStopped(int64_t now) {
        server_speaking = false;
        idle_since_us = now;
    }
    // 丢失连接后可能收不到 speech_stopped，清除旧连接的说话状态。
    // 没有人说话时的重复断线，不算新的用户活动，不能反复重置倒计时。
    constexpr void TransportLost(int64_t now) {
        if (server_speaking)
            SpeechStopped(now);
    }
    constexpr int64_t QuietUs(int64_t now) const {
        return idle_since_us < 0 || server_speaking || now < idle_since_us ? 0
                                                                           : now - idle_since_us;
    }
    constexpr bool ShouldSleep(int64_t now, int64_t timeout_us, bool listening,
                               bool playback_idle) const {
        return listening && playback_idle && idle_since_us >= 0 && !server_speaking &&
               QuietUs(now) >= timeout_us;
    }
};

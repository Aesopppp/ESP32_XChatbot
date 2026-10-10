#pragma once

#include <cstdint>

// ACOS 二进制音频没有 response_id；在接收边界编号，取消后丢弃旧回答，
// 直到服务器的结束/取消确认事件才接受下一段音频。
struct AcosResponseFlow {
    uint32_t current = 0;
    uint32_t discarded = 0;
    uint32_t cancel_requested = 0;
    bool open = false;
    // 本地/云端打断后，必须等用户这句话结束，才允许接收下一轮回答。
    // 取消确认先到、speech_stopped 后到时，夹在中间的旧尾音不能重新开一轮。
    bool waiting_for_speech_end = false;

    constexpr uint32_t Audio() {
        if (waiting_for_speech_end)
            return 0;
        if (!open) {
            ++current;
            open = true;
        }
        return current;
    }
    constexpr uint32_t Done() {
        open = false;
        return current;
    }
    constexpr bool Interrupt() {
        waiting_for_speech_end = true;
        discarded = current;
        const bool send = open && cancel_requested != current;
        if (send)
            cancel_requested = current;
        return send;
    }
    constexpr void SpeechStopped() { waiting_for_speech_end = false; }
    constexpr bool Accepts(uint32_t turn) const { return turn != 0 && turn > discarded; }
};

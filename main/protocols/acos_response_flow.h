#pragma once

#include <cstdint>

// ACOS 二进制音频没有 response_id；在接收边界编号，取消后丢弃旧回答，
// 直到服务器的结束/取消确认事件才接受下一段音频。
struct AcosResponseFlow {
    uint32_t current = 0;  // 当前/最近回答编号，由第一帧音频开启新编号。
    uint32_t discarded = 0;  // 此编号及之前的回答都不可再进入播放队列。
    uint32_t cancel_requested = 0;  // 防止同一回答重复发送取消请求。
    bool open = false;  // 已收到音频但尚未收到结束/取消确认。
    // 本地/云端打断后，必须等用户这句话结束，才允许接收下一轮回答。
    // 取消确认先到、speech_stopped 后到时，夹在中间的旧尾音不能重新开一轮。
    bool waiting_for_speech_end = false;

    // 返回此帧所属编号；用户话音尚未结束时返回 0，拒收夹在中间的尾包。
    constexpr uint32_t Audio() {
        if (waiting_for_speech_end)
            return 0;
        if (!open) {
            ++current;
            open = true;
        }
        return current;
    }
    // 关闭网络回答边界，但不改 discarded；被取消回答的尾包仍然无效。
    constexpr uint32_t Done() {
        open = false;
        return current;
    }
    // 返回 true 表示需要发送一次 response.cancel，false 表示没有可取消的新回答。
    constexpr bool Interrupt() {
        waiting_for_speech_end = true;
        discarded = current;
        const bool send = open && cancel_requested != current;
        if (send)
            cancel_requested = current;
        return send;
    }
    // 用户说完仅移除语音门；旧回答若仍 open，Accepts 仍会拒绝其音频。
    constexpr void SpeechStopped() { waiting_for_speech_end = false; }
    constexpr bool Accepts(uint32_t turn) const { return turn != 0 && turn > discarded; }
};

#include "acos_audio_policy.h"
#include "acos_idle_policy.h"
#include "acos_response_flow.h"

// 交叉编译也运行这些断言；覆盖迟到音频、重复结束、无活动回答时取消等边界。
static_assert(
    [] {
        AcosResponseFlow flow;
        const auto old = flow.Audio();
        if (!flow.Interrupt() || flow.Accepts(old) || flow.Interrupt())
            return false;
        if (flow.Accepts(flow.Audio()))
            return false;
        flow.Done();
        if (flow.Accepts(flow.Audio()))
            return false;
        flow.SpeechStopped();
        const auto next = flow.Audio();
        return next > old && flow.Accepts(next) && !flow.Accepts(old);
    }(),
    "Cancellation must reject late old packets and allow the next answer");

static_assert(
    [] {
        AcosResponseFlow flow;
        const auto buffered = flow.Audio();
        flow.Done();
        flow.Done();
        if (flow.Interrupt() || flow.Accepts(buffered))
            return false;
        flow.SpeechStopped();
        return flow.Accepts(flow.Audio());
    }(),
    "Interrupting buffered completed playback must not cancel a new server response");

static_assert(
    [] {
        AcosResponseFlow flow;
        if (flow.Interrupt() || flow.Accepts(0))
            return false;
        flow.SpeechStopped();
        flow.Audio();
        flow.Interrupt();
        flow.Done();  // 睡眠期间也要处理取消完成的边界。
        flow.SpeechStopped();
        if (!flow.Accepts(flow.Audio()))
            return false;
        flow = {};  // 重连从全新的连接代数开始。
        return flow.current == 0 && !flow.open && flow.Accepts(flow.Audio());
    }(),
    "Wake after sleep/reconnect must not retain the old cancellation barrier");

static_assert(
    [] {
        AcosResponseFlow flow;
        auto old = flow.Audio();
        flow.Interrupt();
        flow.SpeechStopped();  // 用户先说完，但取消确认还没到。
        if (flow.Accepts(flow.Audio()))
            return false;
        flow.Done();
        flow.Done();  // audio.done + response.done 连续到达，不增加轮号。
        auto next = flow.Audio();
        return next == old + 1 && flow.Accepts(next) && !flow.Accepts(old);
    }(),
    "Speech end alone cannot release cancelled audio");

static_assert(
    [] {
        AcosResponseFlow flow;
        flow.Audio();
        flow.Interrupt();  // 本地先打断。
        flow.Done();
        if (flow.Audio() != 0)
            return false;
        if (flow.Interrupt())
            return false;  // 云端随后确认讲话，不重复取消。
        if (flow.Audio() != 0)
            return false;
        flow.SpeechStopped();
        return flow.Accepts(flow.Audio());
    }(),
    "Local interruption plus delayed server speech cannot resume an old tail");

static_assert(
    [] {
        AcosPlaybackBuffer b;
        if (b.Ready(60, false, 1000) || b.Ready(240, false, 100000))
            return false;
        if (!b.Ready(300, false, 200000))
            return false;
        b.delivered = true;
        if (b.Underrun(false) || !b.Underrun(true) || b.Underrun(true))
            return false;
        if (b.Ready(60, false, 300000))
            return false;
        if (!b.Ready(120, true, 310000))
            return false;  // 短尾音结束直接放行。
        b = {};
        if (b.Ready(60, false, 1000))
            return false;
        return b.Ready(60, false, 601000);  // 没有结束事件也不无限等待。
    }(),
    "Jitter buffering must handle startup, underrun, short tails and bounded waits");

// 回归：122.198 秒进入监听后，即使持续上传静音/本地误判 VAD，也应在 182.198 秒到期。
static_assert(
    [] {
        constexpr int64_t timeout = 60000000;
        AcosIdlePolicy idle;
        if (idle.ShouldSleep(999000000, timeout, true, true))
            return false;
        idle.Start(122198000);
        return !idle.ShouldSleep(182197999, timeout, true, true) &&
               idle.ShouldSleep(182198000, timeout, true, true);
    }(),
    "Silence expires at 60 seconds without being extended by local VAD or upload traffic");

static_assert(
    [] {
        constexpr int64_t timeout = 60000000;
        AcosIdlePolicy idle;
        idle.Start(0);
        idle.SpeechStarted(59000000);
        idle.BeginListening(59500000);  // 打断切回监听不能清除正在说话的标志。
        if (idle.ShouldSleep(180000000, timeout, true, true))
            return false;
        idle.SpeechStopped(185000000);
        return !idle.ShouldSleep(244999999, timeout, true, true) &&
               idle.ShouldSleep(245000000, timeout, true, true);
    }(),
    "Confirmed long speech must not sleep mid-sentence and gets a full timeout after stopping");

static_assert(
    [] {
        constexpr int64_t timeout = 60000000;
        AcosIdlePolicy idle;
        idle.Start(1000000);
        if (idle.ShouldSleep(200000000, timeout, false, true) ||
            idle.ShouldSleep(200000000, timeout, true, false))
            return false;                // Speaking 或队列仍在播放都不能超时。
        idle.BeginListening(200000000);  // 音频真正播完之后才重新计时。
        return !idle.ShouldSleep(259999999, timeout, true, true) &&
               idle.ShouldSleep(260000000, timeout, true, true);
    }(),
    "Playback is protected and completion restarts a full listening interval");

static_assert(
    [] {
        constexpr int64_t timeout = 60000000;
        AcosIdlePolicy idle;
        idle.Start(1000000);
        idle.SpeechStarted(5000000);
        idle.TransportLost(10000000);  // 无法收到旧连接 speech_stopped。
        idle.TransportLost(30000000);  // 无人声的重复重连不能延长会话。
        if (!idle.ShouldSleep(70000000, timeout, true, true))
            return false;
        idle.SpeechStarted(71000000);
        idle.Start(90000000);  // 再次唤醒清除旧会话状态。
        return !idle.server_speaking && !idle.ShouldSleep(149999999, timeout, true, true) &&
               idle.ShouldSleep(150000000, timeout, true, true);
    }(),
    "Disconnect and a new session cannot inherit a stuck speech flag or endlessly renew silence");

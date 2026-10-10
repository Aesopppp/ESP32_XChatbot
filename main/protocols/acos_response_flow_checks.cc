#include "acos_audio_policy.h"
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

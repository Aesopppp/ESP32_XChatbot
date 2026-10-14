/*
 * 触屏策略的编译期检查，覆盖上滑、下滑、点击和滑出再滑回。
 * 直接使用生产 TouchPolicy，static_assert 失败会阻止不符合现有手势语义的修改。
 */

#include "vocat_touch_policy.h"

// 对实际使用的识别器进行编译期行为检查，不需要板子或独立测试版算法。
namespace {
constexpr bool TouchChecks() {
    using namespace vocat;
    TouchPolicy p;
    if (p.Update(false, 0, 0).gesture != Gesture::None)
        return false;
    if (p.Update(true, 180, 330).gesture != Gesture::None)
        return false;
    p.Update(true, 181, 240);
    if (p.Update(false, 0, 0).gesture != Gesture::SwipeUp)
        return false;
    if (p.Update(false, 0, 0).gesture != Gesture::None)
        return false;
    p.Update(true, 75, 105);
    p.Update(true, 80, 108);
    auto tap = p.Update(false, 0, 0);
    if (tap.gesture != Gesture::Tap || tap.x != 80 || tap.y != 108)
        return false;
    // 从普通区域向上拖动，以及滑出后返回原点，都不能误点按钮/会话。
    p.Update(true, 180, 230);
    p.Update(true, 180, 100);
    if (p.Update(false, 0, 0).gesture != Gesture::None)
        return false;
    p.Update(true, 75, 105);
    p.Update(true, 150, 105);
    p.Update(true, 75, 105);
    if (p.Update(false, 0, 0).gesture != Gesture::None)
        return false;
    p.Update(true, 180, 40);
    p.Update(true, 180, 140);
    if (p.Update(false, 0, 0).gesture != Gesture::SwipeDown)
        return false;
    p.Update(true, -1, 370);
    if (p.Update(false, 0, 0).gesture != Gesture::None)
        return false;
    p.Update(true, 180, 330);
    p.Cancel();
    return !p.IsPressed() && p.Update(false, 0, 0).gesture == Gesture::None;
}
static_assert(TouchChecks(), "VoCat gestures must not leak swipes into chat taps");
}  // namespace

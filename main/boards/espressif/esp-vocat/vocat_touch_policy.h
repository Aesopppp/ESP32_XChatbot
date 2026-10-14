/*
 * VoCat 360×360 触屏的手势策略：点击、底部上滑、下滑。
 * 只计算坐标和运动距离，不访问 LVGL 或 Application；业务动作由板级触屏流程派发。
 */

#pragma once

#include <algorithm>

namespace vocat {
enum class Gesture { None, Tap, SwipeUp, SwipeDown };
struct TouchResult {
    Gesture gesture = Gesture::None;
    int x = 0;
    int y = 0;
};

// 一次触摸只在松手时提交一次动作；滑出再滑回也不能被误当作点击。
class TouchPolicy {
public:
    constexpr bool IsPressed() const { return pressed_; }
    // 取消尚未松手的识别周期，避免关闭菜单后把旧触摸释放误当作新点击。
    constexpr void Cancel() { pressed_ = false; }
    // 按压阶段只记录起点、终点和最大偏移；松手后最多返回一个手势。
    constexpr TouchResult Update(bool down, int x, int y) {
        if (down) {
            if (x < 0 || x >= 360 || y < 0 || y >= 360)
                return {};
            if (!pressed_) {
                pressed_ = true;
                start_x_ = x;
                start_y_ = y;
                travel_ = 0;
            }
            last_x_ = x;
            last_y_ = y;
            travel_ = std::max(travel_, std::max(Abs(x - start_x_), Abs(y - start_y_)));
            return {};
        }
        if (!pressed_)
            return {};
        pressed_ = false;
        const int dx = last_x_ - start_x_;
        const int dy = last_y_ - start_y_;
        // 上滑必须从底部 60 像素区开始，纵向移动至少 60 像素且大于横向位移。
        if (start_y_ >= 300 && dy <= -60 && -dy > Abs(dx))
            return {Gesture::SwipeUp, last_x_, last_y_};
        if (dy >= 60 && dy > Abs(dx))
            return {Gesture::SwipeDown, last_x_, last_y_};
        // 用全程最大位移判断点击，避免滑动后回到原点被误判为轻触。
        if (travel_ <= 16)
            return {Gesture::Tap, last_x_, last_y_};
        return {};
    }

private:
    static constexpr int Abs(int value) { return value < 0 ? -value : value; }
    bool pressed_ = false;
    int start_x_ = 0, start_y_ = 0, last_x_ = 0, last_y_ = 0, travel_ = 0;
};
}  // namespace vocat

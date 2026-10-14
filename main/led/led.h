/*
 * 指示灯统一接口：应用状态改变时通知具体灯光实现。
 * 无灯设备使用 NoLed 空实现；灯的颜色、亮度或动画只呈现状态，不决定联网和音频行为。
 */

#ifndef _LED_H_
#define _LED_H_

class Led {
public:
    virtual ~Led() = default;
    // Set the led state based on the device state
    // 应用状态更新后调用此纯虚接口，由具体灯实现选择颜色和动画；NoLed 提供无灯设备的空实现。
    virtual void OnStateChanged() = 0;
};


class NoLed : public Led {
public:
    virtual void OnStateChanged() override {}
};

#endif // _LED_H_

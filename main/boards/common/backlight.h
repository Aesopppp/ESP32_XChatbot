/*
 * 屏幕背光控制：恢复亮度、保存用户偏好和渐变过渡。
 * Backlight 管理目标亮度与定时器，PwmBacklight 负责 LEDC 占空比；临时省电不必覆盖 NVS。
 */

#pragma once

#include <cstdint>
#include <functional>

#include <driver/gpio.h>
#include <esp_timer.h>


class Backlight {
public:
    Backlight();
    ~Backlight();

    void RestoreBrightness();
    // permanent=true 请求持久化，false 用于临时亮度；当前值与目标值在渐变期间可能不同。
    void SetBrightness(uint8_t brightness, bool permanent = false);
    inline uint8_t brightness() const { return brightness_; }

protected:
    void OnTransitionTimer();
    // 子类只负责把当前亮度写入硬件，不在此接口重复保存设置或创建计时器。
    virtual void SetBrightnessImpl(uint8_t brightness) = 0;

    esp_timer_handle_t transition_timer_ = nullptr;
    uint8_t brightness_ = 0;
    uint8_t target_brightness_ = 0;
    uint8_t step_ = 1;
};


class PwmBacklight : public Backlight {
public:
    PwmBacklight(gpio_num_t pin, bool output_invert = false, uint32_t freq_hz = 25000);
    ~PwmBacklight();

    void SetBrightnessImpl(uint8_t brightness) override;
};

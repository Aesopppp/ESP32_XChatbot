/*
 * 环形 WS2812 灯带：统一颜色、滚动、呼吸、渐隐等状态动画。
 * 每个像素颜色保存在 colors_，定时器回调在互斥锁内修改像素并刷新灯带。
 */

#ifndef _CIRCULAR_STRIP_H_
#define _CIRCULAR_STRIP_H_

#include "led.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <led_strip.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>
#include <vector>

#define DEFAULT_BRIGHTNESS 32
#define LOW_BRIGHTNESS 4

struct StripColor {
    uint8_t red = 0, green = 0, blue = 0;
};

class CircularStrip : public Led {
public:
    CircularStrip(gpio_num_t gpio, uint16_t max_leds);
    virtual ~CircularStrip();

    void OnStateChanged() override;
    // 亮度参数按 0～255 的 RGB 分量尺度使用，不是菜单中的 0～100 屏幕 / 喇叭百分比。
    void SetBrightness(uint8_t default_brightness, uint8_t low_brightness);
    void SetAllColor(StripColor color);
    void SetSingleColor(uint8_t index, StripColor color);
    void SetMultiColors(const std::vector<StripColor>& colors);
    void Blink(StripColor color, int interval_ms);
    void Breathe(StripColor low, StripColor high, int interval_ms);
    void Scroll(StripColor low, StripColor high, int length, int interval_ms);

private:
    std::mutex mutex_;
    TaskHandle_t blink_task_ = nullptr;
    led_strip_handle_t led_strip_ = nullptr;
    int max_leds_ = 0;
    std::vector<StripColor> colors_;
    int blink_counter_ = 0;
    int blink_interval_ms_ = 0;
    esp_timer_handle_t strip_timer_ = nullptr;
    // 当前动画每个 tick 的步骤，切换动画时替换该回调并重置对应效果状态。
    std::function<void()> strip_callback_ = nullptr;

    uint8_t default_brightness_ = DEFAULT_BRIGHTNESS;
    uint8_t low_brightness_ = LOW_BRIGHTNESS;

    // Per-effect state — reset at the start of each effect call so that
    // re-triggering an effect always starts clean.
    bool       blink_on_        = true;
    bool       breathe_up_      = true;
    StripColor breathe_color_   = {};
    int        scroll_offset_   = 0;

    void StartStripTask(int interval_ms, std::function<void()> cb);
    void Rainbow(StripColor low, StripColor high, int interval_ms);
    void FadeOut(int interval_ms);
};

#endif // _CIRCULAR_STRIP_H_

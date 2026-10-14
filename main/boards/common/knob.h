/*
 * 旋转编码器包装：读取 A / B 相变化并回调旋转方向。
 * OnRotate 的布尔参数为 true 表示 KNOB_RIGHT；旋转对应音量还是其他功能由调用方选择。
 */

#ifndef KNOB_H_
#define KNOB_H_

#include <driver/gpio.h>
#include <functional>
#include <esp_log.h>
#include <iot_knob.h>

class Knob {
public:
    Knob(gpio_num_t pin_a, gpio_num_t pin_b);
    ~Knob();

    void OnRotate(std::function<void(bool)> callback);

private:
    static void knob_callback(void* arg, void* data);

    knob_handle_t knob_handle_;
    gpio_num_t pin_a_;
    gpio_num_t pin_b_;
    std::function<void(bool)> on_rotate_;
};

#endif // KNOB_H_
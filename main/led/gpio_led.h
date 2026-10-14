/*
 * 普通 GPIO 指示灯，通过 LEDC PWM 实现亮度、闪烁和呼吸效果。
 * 渐变结束在中断中发生，只通知专用任务；任务中再处理下一段渐变和互斥锁。
 */

#ifndef _GPIO_LED_H_
#define _GPIO_LED_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "led.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_timer.h>
#include <atomic>
#include <mutex>

class GpioLed : public Led {
 public:
    GpioLed(gpio_num_t gpio);
    GpioLed(gpio_num_t gpio, int output_invert);
    GpioLed(gpio_num_t gpio, int output_invert, ledc_timer_t timer_num, ledc_channel_t channel);
    virtual ~GpioLed();

    void OnStateChanged() override;
    void TurnOn();
    void TurnOff();
    void SetBrightness(uint8_t brightness);

 private:
    std::mutex mutex_;
    TaskHandle_t blink_task_ = nullptr;
    ledc_channel_config_t ledc_channel_ = {0};
    bool ledc_initialized_ = false;
    uint32_t duty_ = 0;
    int blink_counter_ = 0;
    int blink_interval_ms_ = 0;
    esp_timer_handle_t blink_timer_ = nullptr;
    // 记录下一段渐变方向；渐变结束后由工作任务翻转，实现呼吸效果。
    bool fade_up_ = true;
    TaskHandle_t event_task_handle_;
    
    static void EventTask(void* arg);
    void StartBlinkTask(int times, int interval_ms);
    void OnBlinkTimer();

    void BlinkOnce();
    void Blink(int times, int interval_ms);
    void StartContinuousBlink(int interval_ms);
    void StartFadeTask();
    void OnFadeEnd();
    // LEDC ISR 桥接，只通知 EventTask；不在中断中持有 std::mutex。
    static bool IRAM_ATTR FadeCallback(const ledc_cb_param_t *param, void *user_arg);
};

#endif  // _GPIO_LED_H_

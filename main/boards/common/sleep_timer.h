/*
 * 备用的轻睡眠 / 深睡眠计时器。
 * 硬件睡眠会暂停部分外设，需由使用它的板级代码配置唤醒源；不是 ACOS 保持连接的对话待机。
 * 轻睡眠路径包含阻塞循环，当前 VoCat 对话逻辑不能直接用它替代音频上传门控。
 */

#pragma once

#include <functional>

#include <esp_timer.h>
#include <esp_pm.h>

class SleepTimer {
public:
    SleepTimer(int seconds_to_light_sleep = 20, int seconds_to_deep_sleep = -1);
    ~SleepTimer();

    void SetEnabled(bool enabled);
    void OnEnterLightSleepMode(std::function<void()> callback);
    void OnExitLightSleepMode(std::function<void()> callback);
    void OnEnterDeepSleepMode(std::function<void()> callback);
    void WakeUp();

private:
    void CheckTimer();

    esp_timer_handle_t sleep_timer_ = nullptr;
    bool enabled_ = false;
    int ticks_ = 0;
    int seconds_to_light_sleep_;
    int seconds_to_deep_sleep_;
    bool in_light_sleep_mode_ = false;

    std::function<void()> on_enter_light_sleep_mode_;
    std::function<void()> on_exit_light_sleep_mode_;
    std::function<void()> on_enter_deep_sleep_mode_;
};

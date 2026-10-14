/*
 * 通用低功耗倒计时：检查空闲后请求省电或关机。
 * 此计时器可关闭采集 / 唤醒并调整 CPU 频率，与 ACOS 的 60 秒对话待机策略不同。
 * 阈值 -1 表示禁用相应阶段，是否实际采用本模块取决于板级代码。
 */

#pragma once

#include <functional>

#include <esp_timer.h>
#include <esp_pm.h>

class PowerSaveTimer {
public:
    PowerSaveTimer(int cpu_max_freq, int seconds_to_sleep = 20, int seconds_to_shutdown = -1);
    ~PowerSaveTimer();

    void SetEnabled(bool enabled);
    void OnEnterSleepMode(std::function<void()> callback);
    void OnExitSleepMode(std::function<void()> callback);
    void OnShutdownRequest(std::function<void()> callback);
    void WakeUp();

private:
    void PowerSaveCheck();

    esp_timer_handle_t power_save_timer_ = nullptr;
    bool enabled_ = false;
    bool in_sleep_mode_ = false;
    bool is_wake_word_running_ = false;
    int ticks_ = 0;
    int cpu_max_freq_;
    int seconds_to_sleep_;
    int seconds_to_shutdown_;

    std::function<void()> on_enter_sleep_mode_;
    std::function<void()> on_exit_sleep_mode_;
    std::function<void()> on_shutdown_request_;
};

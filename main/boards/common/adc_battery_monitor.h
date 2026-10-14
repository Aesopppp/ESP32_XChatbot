/*
 * ADC 电池估算包装：按分压电阻配置采样并查询容量 / 充电状态。
 * 估算依赖硬件分压和电池模型；失败时返回默认电量，不能将该值作为精确电量计结果。
 */

#ifndef ADC_BATTERY_MONITOR_H
#define ADC_BATTERY_MONITOR_H

#include <functional>
#include <driver/gpio.h>
#include <adc_battery_estimation.h>
#include <esp_timer.h>

class AdcBatteryMonitor {
public:
    AdcBatteryMonitor(adc_unit_t adc_unit, adc_channel_t adc_channel, float upper_resistor, float lower_resistor, gpio_num_t charging_pin = GPIO_NUM_NC);
    ~AdcBatteryMonitor();

    bool IsCharging();
    bool IsDischarging();
    uint8_t GetBatteryLevel();

    void OnChargingStatusChanged(std::function<void(bool)> callback);

private:
    gpio_num_t charging_pin_;
    adc_battery_estimation_handle_t adc_battery_estimation_handle_ = nullptr;
    esp_timer_handle_t timer_handle_ = nullptr;
    bool is_charging_ = false;
    std::function<void(bool)> on_charging_status_changed_;

    void CheckBatteryStatus();
};

#endif // ADC_BATTERY_MONITOR_H

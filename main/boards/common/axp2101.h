/*
 * AXP2101 电源管理芯片的寄存器访问封装。
 * 提供充放电、电量、温度和关机查询 / 控制，只有板上实际使用该芯片时才有意义。
 */

#ifndef __AXP2101_H__
#define __AXP2101_H__

#include "i2c_device.h"

class Axp2101 : public I2cDevice {
public:
    Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    bool IsCharging();
    bool IsDischarging();
    bool IsChargingDone();
    int GetBatteryLevel();
    float GetTemperature();
    void PowerOff();

private:
    int GetBatteryCurrentDirection();
};

#endif

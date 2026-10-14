/*
 * SY6970 充电 / 电源芯片辅助驱动。
 * 读取充电状态、电池电压与目标充电电压，并提供关机控制；寄存器定义与芯片型号绑定。
 */

#ifndef __SY6970_H__
#define __SY6970_H__

#include "i2c_device.h"

class Sy6970 : public I2cDevice {
public:
    Sy6970(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    bool IsCharging();
    bool IsPowerGood();
    bool IsChargingDone();
    int GetBatteryLevel();
    void PowerOff();

private:
    int GetChangingStatus();
    int GetBatteryVoltage();
    int GetChargeTargetVoltage();
};

#endif
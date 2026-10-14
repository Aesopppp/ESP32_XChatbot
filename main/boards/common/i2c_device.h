/*
 * I2C 寄存器设备基础类，供电源芯片等公共驱动复用。
 * 借用板级 I2C 总线，注册 7 位地址、400 kHz 的从设备；寄存器传输超时为 100 毫秒。
 * 错误由 ESP_ERROR_CHECK 处理，ResetBus 重置共享总线，不等同于只重置某一颗芯片。
 */

#ifndef I2C_DEVICE_H
#define I2C_DEVICE_H

#include <driver/i2c_master.h>

class I2cDevice {
public:
    I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr);

protected:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t i2c_device_;
    uint8_t device_address_;

    void WriteReg(uint8_t reg, uint8_t value);
    void WriteRegs(uint8_t reg, const uint8_t* buffer, size_t length);
    uint8_t ReadReg(uint8_t reg);
    void ReadRegs(uint8_t reg, uint8_t* buffer, size_t length);
    esp_err_t ResetBus(const char* reason);
};

#endif  // I2C_DEVICE_H

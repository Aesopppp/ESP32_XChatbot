/*
 * I2C 寄存器设备基础类，供电源芯片等公共驱动复用。
 * 借用板级 I2C 总线，注册 7 位地址、400 kHz 的从设备；寄存器传输超时为 100 毫秒。
 * 错误由 ESP_ERROR_CHECK 处理，ResetBus 重置共享总线，不等同于只重置某一颗芯片。
 */

#include "i2c_device.h"

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#define TAG "I2cDevice"

I2cDevice::I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : i2c_bus_(i2c_bus), device_address_(addr) {
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags =
            {
                .disable_ack_check = 0,
            },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_));
    assert(i2c_device_ != NULL);
}

void I2cDevice::WriteReg(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, 2, 100));
}

// 一次传输先发送寄存器地址再发送数据，临时拼接缓冲只在本次同步事务内有效。
void I2cDevice::WriteRegs(uint8_t reg, const uint8_t* buffer, size_t length) {
    uint8_t* tx = (uint8_t*)malloc(length + 1);
    if (tx == nullptr) {
        return;
    }
    tx[0] = reg;
    memcpy(tx + 1, buffer, length);
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, tx, length + 1, 100));
    free(tx);
}

uint8_t I2cDevice::ReadReg(uint8_t reg) {
    uint8_t buffer[1];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100));
    return buffer[0];
}

// 先写寄存器地址再读取连续数据，读取结果写入调用方提供的缓冲区。
void I2cDevice::ReadRegs(uint8_t reg, uint8_t* buffer, size_t length) {
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, length, 100));
}

// 重置整个共享 I2C 总线并记录原因；调用方需协调其他正在使用此总线的外设。
esp_err_t I2cDevice::ResetBus(const char* reason) {
    ESP_LOGW(TAG, "Resetting I2C bus: %s", reason ? reason : "unspecified");
    return i2c_master_bus_reset(i2c_bus_);
}

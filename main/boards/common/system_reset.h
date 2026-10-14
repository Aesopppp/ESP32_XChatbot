/*
 * 物理按键触发的 NVS 清除、OTA 启动记录清除及延时重启工具。
 * 这些接口会丢失持久化设置，属于显式恢复操作，不能拿来处理普通 Wi-Fi 重连。
 */

#ifndef _SYSTEM_RESET_H
#define _SYSTEM_RESET_H

#include <driver/gpio.h>

class SystemReset {
public:
    SystemReset(gpio_num_t reset_nvs_pin, gpio_num_t reset_factory_pin); // 构造函数私有化
    void CheckButtons();

private:
    gpio_num_t reset_nvs_pin_;
    gpio_num_t reset_factory_pin_;

    void ResetNvsFlash();
    void ResetToFactory();
    void RestartInSeconds(int seconds);
};


#endif

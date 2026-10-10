#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"

#define TAG "main"

// ESP-IDF 启动入口。extern "C" 保留 C 链接名，供 SDK 启动代码调用。
// 启动顺序：NVS 持久化存储 -> 板级/音频/网络初始化 -> 主事件循环。
// Wi-Fi 连接和 ACOS 鉴权是异步过程，Initialize 返回不等于已能对话。
extern "C" void app_main(void)
{
    // NVS 保存 Wi-Fi、音量和服务配置。仅在空间/版本异常时擦除并重建；
    // 此恢复操作会清除已保存设置，因此正常启动不会执行擦除。
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Application 为设备生命周期内唯一的应用对象；Run 持续处理事件，不返回。
    auto& app = Application::GetInstance();
    app.Initialize();
    app.Run();  // This function runs the main event loop and never returns
}

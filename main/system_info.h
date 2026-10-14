/*
 * 系统诊断接口：芯片、Flash、堆、任务负载和电源锁信息。
 * 这些数据用于状态展示与排障；堆的历史最低值不等于当前空闲值，Flash 总量不等于可用应用分区。
 */

#ifndef _SYSTEM_INFO_H_
#define _SYSTEM_INFO_H_

#include <string>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

class SystemInfo {
public:
    // 返回检测的 Flash 容量，不扣除引导、资源或其他分区占用。
    static size_t GetFlashSize();
    // 默认堆的历史最低空闲量；要区分内部 RAM 和 PSRAM，需另看对应 capability 指标。
    static size_t GetMinimumFreeHeapSize();
    static size_t GetFreeHeapSize();
    static std::string GetMacAddress();
    static std::string GetChipModelName();
    static std::string GetUserAgent();
    // 采样期间会等待 xTicksToWait 个系统 tick，不能放到实时音频或主循环热路径。
    static esp_err_t PrintTaskCpuUsage(TickType_t xTicksToWait);
    static void PrintTaskList();
    static void PrintHeapStats();
    static void PrintPmLocks();
};

#endif // _SYSTEM_INFO_H_

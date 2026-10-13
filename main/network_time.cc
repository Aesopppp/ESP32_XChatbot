#include "network_time.h"
#include "sdkconfig.h"

#if CONFIG_USE_ACOS
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_timer.h>
#include <atomic>
#include <cstdlib>
#include <ctime>

namespace NetworkTime {
namespace {
constexpr const char* kTag = "NetworkTime";
constexpr const char* kServers[] = {"ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org"};
constexpr int64_t kRetryUs = 45LL * 1000000;
bool online = false;
bool initialized = false;
bool waiting = false;
size_t server_index = 0;
int64_t attempt_started_us = 0;
std::atomic<uint32_t> sync_count{0};
uint32_t observed_sync_count = 0;

void OnTimeSync(timeval* tv) {
    // SNTP 已将 UTC 时间写入系统。时区只在 localtime_r 时转换，不能再加8小时。
    ++sync_count;
    tm local{};
    localtime_r(&tv->tv_sec, &local);
    char text[32];
    strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
    ESP_LOGI(kTag, "Time synchronized: %s CST (UTC+8)", text);
}

void StartAttempt() {
    if (initialized) {
        esp_netif_sntp_deinit();
        initialized = false;
    }
    // 每次使用一个服务器，兼容工程默认的 SNTP_MAX_SERVERS=1。
    // 等待超时才轮换备用服务器，不修改生成的 sdkconfig。
    observed_sync_count = sync_count.load();
    attempt_started_us = esp_timer_get_time();
    waiting = true;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(kServers[server_index]);
    config.wait_for_sync = false;
    config.sync_cb = OnTimeSync;
    const auto err = esp_netif_sntp_init(&config);
    initialized = err == ESP_OK;
    if (initialized)
        ESP_LOGI(kTag, "Requesting network time from %s", kServers[server_index]);
    else
        ESP_LOGW(kTag, "Cannot start time sync: %s; will retry", esp_err_to_name(err));
}
}  // namespace

void OnConnected() {
    if (online)
        return;
    // POSIX 时区字符串符号与 UTC 偏移相反：CST-8 表示北京时间 UTC+8。
    setenv("TZ", "CST-8", 1);
    tzset();
    online = true;
    server_index = 0;
    StartAttempt();
}

void OnDisconnected() {
    online = false;
    waiting = false;
    if (initialized) {
        esp_netif_sntp_deinit();
        initialized = false;
    }
    // 不清除已经校准的系统时间；掉线期间继续走时，重新联网再校准。
}

void Poll() {
    if (!online || !waiting)
        return;
    if (sync_count.load() != observed_sync_count) {
        waiting = false;
        // 后续定期校准交给 SNTP，本工程默认每小时一次。
        return;
    }
    if (esp_timer_get_time() - attempt_started_us >= kRetryUs) {
        ESP_LOGW(kTag, "No time reply from %s; trying next server", kServers[server_index]);
        server_index = (server_index + 1) % (sizeof(kServers) / sizeof(kServers[0]));
        StartAttempt();
    }
}
}  // namespace NetworkTime
#endif

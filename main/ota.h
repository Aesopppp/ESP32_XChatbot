/*
 * 固件 OTA 与设备激活接口：查询版本、接收服务配置、下载新固件并确认启动成功。
 * 这是通用平台路径；ACOS 是否跳过激活 / 版本查询由 Application 的启动分支决定。
 * 升级写入 OTA 应用分区，与 Assets 的模型、字体、表情资源下载分别管理。
 */

#ifndef _OTA_H
#define _OTA_H

#include <expected>
#include <functional>
#include <string>
#include <vector>

#include <esp_err.h>
#include "board.h"
#include "network_error.h"

class Ota {
public:
    Ota();
    ~Ota();

    // 查询版本及平台配置，以 NetworkResult 区分网络故障与成功结果。
    NetworkResult<> CheckVersion();
    // 执行通用平台激活挑战流程，不是 ACOS 的 WebSocket token 鉴权。
    esp_err_t Activate();
    bool HasActivationChallenge() { return has_activation_challenge_; }
    bool HasNewVersion() { return has_new_version_; }
    bool HasMqttConfig() { return has_mqtt_config_; }
    bool HasWebsocketConfig() { return has_websocket_config_; }
    bool HasActivationCode() { return has_activation_code_; }
    bool HasServerTime() { return has_server_time_; }
    bool StartUpgrade(std::function<void(int progress, size_t speed)> callback);
    // 通过 URL 下载固件；回调参数分别为百分比进度和字节速度。
    static bool Upgrade(const std::string& firmware_url, std::function<void(int progress, size_t speed)> callback);
    // 确认当前启动固件可用，避免 OTA 回滚策略把已成功启动版本视为失败。
    void MarkCurrentVersionValid();

    const std::string& GetFirmwareVersion() const { return firmware_version_; }
    const std::string& GetCurrentVersion() const { return current_version_; }
    const std::string& GetFirmwareUrl() const { return firmware_url_; }
    const std::string& GetActivationMessage() const { return activation_message_; }
    const std::string& GetActivationCode() const { return activation_code_; }
    std::string GetCheckVersionUrl();

private:
    std::string activation_message_;
    std::string activation_code_;
    bool has_new_version_ = false;
    bool has_mqtt_config_ = false;
    bool has_websocket_config_ = false;
    bool has_server_time_ = false;
    bool has_activation_code_ = false;
    bool has_serial_number_ = false;
    bool has_activation_challenge_ = false;
    std::string current_version_;
    std::string firmware_version_;
    std::string firmware_url_;
    std::string activation_challenge_;
    std::string serial_number_;
    int activation_timeout_ms_ = 30000;

    std::function<void(int progress, size_t speed)> upgrade_callback_;
    std::expected<std::vector<int>, std::string> ParseVersion(const std::string& version);
    bool IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion);
    std::string GetActivationPayload();
    std::unique_ptr<Http> SetupHttp();
};

#endif // _OTA_H

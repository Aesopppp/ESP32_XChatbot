/*
 * 可选音频调试输出：把 PCM 数据以 UDP 发往配置的主机。
 * 只在音频调试配置启用时创建套接字；这条输出不是 ACOS 的 WebSocket 音频协议。
 */

#include "audio_debugger.h"

#include "sdkconfig.h"

#if CONFIG_USE_AUDIO_DEBUGGER
#include <arpa/inet.h>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <errno.h>
#include <esp_log.h>
#include <string>
#include <system_error>
#include <unistd.h>
#endif

#define TAG "AudioDebugger"

AudioDebugger::AudioDebugger() {
#if CONFIG_USE_AUDIO_DEBUGGER
    udp_sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd_ >= 0) {
        std::string server_addr = CONFIG_AUDIO_DEBUG_UDP_SERVER;
        size_t colon_pos = server_addr.find(':');

        if (colon_pos != std::string::npos) {
            std::string ip = server_addr.substr(0, colon_pos);
            std::string port_str = server_addr.substr(colon_pos + 1);
            int port = 0;
            auto [ptr, ec] =
                std::from_chars(port_str.data(), port_str.data() + port_str.size(), port);

            if (ec == std::errc() && ptr == port_str.data() + port_str.size() && port > 0 &&
                port <= UINT16_MAX) {
                memset(&udp_server_addr_, 0, sizeof(udp_server_addr_));
                udp_server_addr_.sin_family = AF_INET;
                udp_server_addr_.sin_port = htons(port);
                inet_pton(AF_INET, ip.c_str(), &udp_server_addr_.sin_addr);

                ESP_LOGI(TAG, "Initialized server address: %s", CONFIG_AUDIO_DEBUG_UDP_SERVER);
            } else {
                ESP_LOGW(TAG, "Invalid server address: %s, should be IP:PORT",
                         CONFIG_AUDIO_DEBUG_UDP_SERVER);
                close(udp_sockfd_);
                udp_sockfd_ = -1;
            }
        } else {
            ESP_LOGW(TAG, "Invalid server address: %s, should be IP:PORT", CONFIG_AUDIO_DEBUG_UDP_SERVER);
            close(udp_sockfd_);
            udp_sockfd_ = -1;
        }
    } else {
        ESP_LOGW(TAG, "Failed to create UDP socket: %d", errno);
    }
#endif
}

AudioDebugger::~AudioDebugger() {
#if CONFIG_USE_AUDIO_DEBUGGER
    if (udp_sockfd_ >= 0) {
        close(udp_sockfd_);
        ESP_LOGI(TAG, "Closed UDP socket");
    }
#endif
}

// 将本次 int16_t PCM 按字节发送到调试端，使用完仍由调用方管理原始数据。
void AudioDebugger::Feed(const std::vector<int16_t>& data) {
#if CONFIG_USE_AUDIO_DEBUGGER
    if (udp_sockfd_ >= 0) {
        ssize_t sent = sendto(udp_sockfd_, data.data(), data.size() * sizeof(int16_t), 0,
            reinterpret_cast<struct sockaddr*>(&udp_server_addr_), sizeof(udp_server_addr_));
        if (sent < 0) {
            ESP_LOGW(TAG, "Failed to send audio data to %s: %d", CONFIG_AUDIO_DEBUG_UDP_SERVER, errno);
        } else {
            ESP_LOGD(TAG, "Sent %d bytes audio data to %s", sent, CONFIG_AUDIO_DEBUG_UDP_SERVER);
        }
    }
#endif
}

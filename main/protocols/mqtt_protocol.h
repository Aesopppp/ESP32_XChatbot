/*
 * 标准小智 MQTT / UDP 传输：MQTT 承载控制消息，UDP 承载加密音频。
 * AES-CTR 密钥及 nonce 由服务 hello 提供；序列号用于过滤重复或迟到音频。
 * 这是可选后端，与当前 ACOS WebSocket 直连路径的认证、消息与空闲策略不同。
 */

#ifndef MQTT_PROTOCOL_H
#define MQTT_PROTOCOL_H


#include "protocol.h"
#include <mqtt.h>
#include <udp.h>
#include <cJSON.h>
#include <psa/crypto.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_timer.h>

#include <functional>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>

#define MQTT_PING_INTERVAL_SECONDS 90
#define MQTT_RECONNECT_INTERVAL_MS 60000

#define MQTT_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

class MqttProtocol : public Protocol {
public:
    MqttProtocol();
    ~MqttProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

private:
    // Alive flag for safe scheduled callbacks - set to false in destructor
    // 已派发到应用主任务的回调先检查存活标志，避免析构后继续访问 this。
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    
    EventGroupHandle_t event_group_handle_;

    std::string publish_topic_;

    // 保护音频通道、会话序列等状态；加密操作使用另一个锁。
    mutable std::mutex channel_mutex_;
    std::mutex crypto_mutex_;
    std::unique_ptr<Mqtt> mqtt_;
    // Shared so SendAudio() can hold a reference and call Send() without
    // holding channel_mutex_; the UDP receive callback (run on the modem's
    // AT event task) also needs that mutex, so holding it across a blocking
    // Send() would stall AT response parsing and cause spurious timeouts.
    // 发送前在锁内复制 shared_ptr，再在锁外 Send，避免阻塞接收 AT 事件也需要的通道锁。
    std::shared_ptr<Udp> udp_;
    // PSA 密钥句柄与 IV / nonce 配合加密；销毁密钥需保持与加密操作互斥。
    psa_key_id_t aes_key_id_ = PSA_KEY_ID_NULL;
    std::string aes_nonce_;
    std::string udp_server_;
    int udp_port_;
    uint32_t local_sequence_;
    uint32_t remote_sequence_;
    esp_timer_handle_t reconnect_timer_;

    bool StartMqttClient(bool report_error=false);
    void ParseServerHello(const cJSON* root);
    bool DecodeHexString(const std::string& hex_string, std::string& decoded);
    bool CryptAesCtr(const uint8_t* input, size_t input_size, const uint8_t* nonce, uint8_t* output);

    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
};


#endif // MQTT_PROTOCOL_H

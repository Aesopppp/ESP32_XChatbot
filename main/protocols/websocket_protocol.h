/*
 * 标准小智 WebSocket 传输，可选备用实现，不是当前 ACOS 的消息格式。
 * hello 交换协商会话和音频参数；二进制封装随协议版本变化。
 * 物理 WebSocket 的连接与逻辑音频通道开启由此后端管理，上层只依赖 Protocol 接口。
 */

#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_


#include "protocol.h"

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

class WebsocketProtocol : public Protocol {
public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

private:
    // 等待服务端 hello 的同步信号，不代表已经播放完成。
    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<WebSocket> websocket_;
    // 决定二进制头格式；服务配置选择版本，不能按 Opus 负载猜测包头。
    int version_ = 1;

    void ParseServerHello(const cJSON* root);
    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
};

#endif

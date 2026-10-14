/*
 * 可选音频调试输出：把 PCM 数据以 UDP 发往配置的主机。
 * 只在音频调试配置启用时创建套接字；这条输出不是 ACOS 的 WebSocket 音频协议。
 */

#ifndef AUDIO_DEBUGGER_H
#define AUDIO_DEBUGGER_H

#include <cstdint>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>

class AudioDebugger {
public:
    AudioDebugger();
    ~AudioDebugger();

    void Feed(const std::vector<int16_t>& data);

private:
    int udp_sockfd_ = -1;
    struct sockaddr_in udp_server_addr_;
};

#endif

#pragma once

#include "acos_audio_policy.h"
#include "acos_response_flow.h"
#include "protocol.h"

#include <esp_heap_caps.h>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

// ACOS 适配器把服务端事件转换为 Application 使用的统一协议回调。
// ready_ 表示已鉴权，awake_ 表示本地已唤醒；二者均为真才上传麦克风。
// 底层 WebSocket 在待唤醒时仍保留，唤醒无需重新完成 TLS 握手。
// 网络工作线程独占 WebSocket；回调只入队，主任务按接收顺序处理状态和音频。
// 所有队列有容量限制，慢网络不会无限占用内存或阻塞音频/界面任务。
class AcosProtocol : public Protocol {
public:
    ~AcosProtocol() override;
    bool Start() override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    void SendWakeWordDetected(const std::string& wake_word) override;
    void SendStartListening(ListeningMode mode) override;
    void SendStopListening() override;
    void SendAbortSpeaking(AbortReason reason) override;
    void SendMcpMessage(const std::string& message) override;
    void Poll() override;
    void SetPlaybackReady(std::function<bool()> callback) { playback_ready_ = std::move(callback); }
    void SetPlaybackIdle(std::function<bool()> callback) { playback_idle_ = std::move(callback); }

private:
    // 下行帧：PSRAM 存原始二进制 Opus 或 JSON，turn 绑定本地回答编号。
    // 自定义删除器确保分配到 ESP 堆的内存按相同接口释放。
    struct Frame {
        std::unique_ptr<uint8_t, decltype(&heap_caps_free)> data{nullptr, heap_caps_free};
        size_t size = 0;
        bool binary = false;
        bool done = false;
        uint32_t turn = 0;
    };
    // 上行帧：创建时间限制最大排队延迟，epoch 过滤休眠/重连前的旧录音。
    struct Upload {
        std::string json;
        int64_t created_us = 0;
        uint32_t epoch = 0;
    };
    // 取消请求同时绑定回答编号与连接代数，避免延迟请求取消下一轮回答。
    struct Cancel {
        uint32_t turn;
        uint32_t generation;
    };
    std::thread worker_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> awake_{false};
    std::atomic<bool> reconnect_{false};
    std::atomic<uint32_t> epoch_{0};
    std::atomic<int64_t> last_receive_us_{0};
    // 下列队列及回答状态跨接收回调、Worker、Poll 共享，访问时持有此锁。
    // 实际网络发送和应用回调在锁外执行，避免阻塞队列或产生重入死锁。
    std::mutex queue_mutex_;
    std::deque<Frame> incoming_;
    std::deque<Upload> outgoing_;
    std::deque<Cancel> cancels_;
    size_t incoming_bytes_ = 0;
    AcosResponseFlow flow_;
    uint32_t generation_ = 0;
    uint32_t playback_turn_ = 0;
    bool interrupted_ = false;
    bool connection_changed_ = false;
    AcosPlaybackBuffer playback_buffer_;  // 预缓冲策略，不存储实际音频数据。
    bool playback_finished_ = false;
    uint32_t rx_audio_count_ = 0;
    uint32_t stale_audio_count_ = 0;
    uint32_t upload_drops_ = 0;
    uint32_t underruns_ = 0;
    int64_t last_audio_us_ = 0;
    int64_t max_audio_gap_us_ = 0;
    int64_t last_stats_us_ = 0;
    std::string url_;
    std::string token_;
    std::function<bool()> playback_ready_;
    std::function<bool()> playback_idle_;

    bool SendText(const std::string& text) override;
    void Worker();
    void Receive(const char* data, size_t size, bool binary);
    void Emit(const char* type, const char* state = nullptr, const char* text = nullptr);
    void InterruptLocked();
};

/*
 * 应用与网络传输之间的统一接口，管理音频包、回调及通用小智控制消息。
 * 本文件默认 JSON 格式用于标准小智传输；ACOS 子类覆盖相关方法以使用 ACOS 事件协议。
 * 网络回调不保证运行在主任务，涉及应用状态时应交给 Schedule 或主循环处理。
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cJSON.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// 传输与音频服务之间的单包契约：payload 是 Opus 压缩数据，不是 PCM/WAV。
// sample_rate 单位 Hz，frame_duration/media_position 单位 ms；playback_id 关联通知字幕。
struct AudioStreamPacket {
    int sample_rate = 0;
    int frame_duration = 0;
    uint32_t timestamp = 0;
    uint32_t playback_id = 0;
    uint32_t media_position_ms = 0;
    std::vector<uint8_t> payload;
};

// 标准小智 v2 二进制头，packed 保证线格式没有编译器填充字节。
struct BinaryProtocol2 {
    uint16_t version;
    uint16_t type;          // Message type (0: OPUS, 1: JSON)
    uint32_t reserved;      // Reserved for future use
    uint32_t timestamp;     // Timestamp in milliseconds (used for server-side AEC)
    uint32_t payload_size;  // Payload size in bytes
    uint8_t payload[];      // Payload data
} __attribute__((packed));

// 标准小智 v3 使用更短的二进制头；不是 ACOS 的裸 Opus 下行格式。
struct BinaryProtocol3 {
    uint8_t type;
    uint8_t reserved;
    uint16_t payload_size;
    uint8_t payload[];
} __attribute__((packed));

enum AbortReason { kAbortReasonNone, kAbortReasonWakeWordDetected };

enum ListeningMode {
    kListeningModeAutoStop,
    kListeningModeManualStop,
    kListeningModeRealtime  // 需要 AEC 支持
};

// 协议抽象：ACOS、标准 WebSocket、MQTT/UDP 复用同一应用层回调。
// 物理连接与逻辑音频通道不同，CloseAudioChannel 不一定断开网络。
// JSON 回调中的 cJSON 指针是借用值；跨任务使用须复制字段或复制 JSON。
class Protocol {
public:
    virtual ~Protocol() = default;

    inline int server_sample_rate() const { return server_sample_rate_; }
    inline int server_frame_duration() const { return server_frame_duration_; }
    inline const std::string& session_id() const { return session_id_; }

    void OnIncomingAudio(std::function<void(std::unique_ptr<AudioStreamPacket> packet)> callback);
    void OnIncomingJson(std::function<void(const cJSON* root)> callback);
    void OnAudioChannelOpened(std::function<void()> callback);
    void OnAudioChannelClosed(std::function<void()> callback);
    void OnNetworkError(std::function<void(const std::string& message)> callback);
    void OnConnected(std::function<void()> callback);
    void OnDisconnected(std::function<void()> callback);

    virtual bool Start() = 0;
    // 默认传输由回调驱动；有界队列适配器可在主任务中分批处理输入。
    virtual void Poll() {}
    virtual bool OpenAudioChannel() = 0;
    virtual void CloseAudioChannel(bool send_goodbye = true) = 0;
    virtual bool IsAudioChannelOpened() const = 0;
    // unique_ptr 将音频包所有权转给协议；返回语义由具体后端决定，不等同于云端处理完成。
    virtual bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) = 0;
    virtual void SendWakeWordDetected(const std::string& wake_word);
    virtual void SendStartListening(ListeningMode mode);
    virtual void SendStopListening();
    virtual void SendAbortSpeaking(AbortReason reason);
    virtual void SendMcpMessage(const std::string& message);

protected:
    std::function<void(const cJSON* root)> on_incoming_json_;
    std::function<void(std::unique_ptr<AudioStreamPacket> packet)> on_incoming_audio_;
    std::function<void()> on_audio_channel_opened_;
    std::function<void()> on_audio_channel_closed_;
    std::function<void(const std::string& message)> on_network_error_;
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;

    int server_sample_rate_ = 24000;
    int server_frame_duration_ = 60;
    bool error_occurred_ = false;
    std::string session_id_;
    std::chrono::time_point<std::chrono::steady_clock> last_incoming_time_;

    virtual bool SendText(const std::string& text) = 0;
    virtual void SetError(const std::string& message);
    void SetError(const std::string& message, const std::string& detail);
    virtual bool IsTimeout() const;
    static void AddTextFontCapabilities(cJSON* root);
};

#endif  // PROTOCOL_H

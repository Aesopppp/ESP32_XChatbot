/*
 * 应用与网络传输之间的统一接口，管理音频包、回调及通用小智控制消息。
 * 本文件默认 JSON 格式用于标准小智传输；ACOS 子类覆盖相关方法以使用 ACOS 事件协议。
 * 网络回调不保证运行在主任务，涉及应用状态时应交给 Schedule 或主循环处理。
 */

#include "protocol.h"
#include "assets.h"

#include <esp_log.h>

#define TAG "Protocol"

// 读取已应用的字体能力并补充 hello JSON；新建 font 子节点转交 root 所有。
void Protocol::AddTextFontCapabilities(cJSON* root) {
    auto capability = Assets::GetInstance().text_font_capability();
    cJSON* features = cJSON_GetObjectItem(root, "features");
    if (cJSON_IsObject(features)) {
        cJSON_AddBoolToObject(features, "glyph_push", capability.glyph_push);
    }

    if (!capability.glyph_push) {
        return;
    }
    cJSON* font = cJSON_CreateObject();
    cJSON_AddStringToObject(font, "bundle", capability.bundle.c_str());
    cJSON_AddStringToObject(font, "charset", capability.charset.c_str());
    cJSON_AddNumberToObject(font, "size", capability.size);
    cJSON_AddNumberToObject(font, "bpp", capability.bpp);
    cJSON_AddItemToObject(root, "text_font", font);
}

// 保存控制消息回调；root 只在回调期间有效，跨线程消费前需复制。
void Protocol::OnIncomingJson(std::function<void(const cJSON* root)> callback) {
    on_incoming_json_ = callback;
}

// 保存音频回调；unique_ptr 的移动会把包和 payload 寿命交给接收方。
void Protocol::OnIncomingAudio(
    std::function<void(std::unique_ptr<AudioStreamPacket> packet)> callback) {
    on_incoming_audio_ = callback;
}

void Protocol::OnAudioChannelOpened(std::function<void()> callback) {
    on_audio_channel_opened_ = callback;
}

void Protocol::OnAudioChannelClosed(std::function<void()> callback) {
    on_audio_channel_closed_ = callback;
}

void Protocol::OnNetworkError(std::function<void(const std::string& message)> callback) {
    on_network_error_ = callback;
}

void Protocol::OnConnected(std::function<void()> callback) { on_connected_ = callback; }

void Protocol::OnDisconnected(std::function<void()> callback) { on_disconnected_ = callback; }

// 置错误标志并通知上层；不会在此函数内自动执行断线、重连或 UI 更新。
void Protocol::SetError(const std::string& message) {
    error_occurred_ = true;
    if (on_network_error_ != nullptr) {
        on_network_error_(message);
    }
}

void Protocol::SetError(const std::string& message, const std::string& detail) {
    if (detail.empty()) {
        SetError(message);
        return;
    }
    SetError(message + "\n" + detail);
}

// 默认发送标准小智 abort 事件；ACOS 使用自己的 response.cancel 流程。
void Protocol::SendAbortSpeaking(AbortReason reason) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"abort\"";
    if (reason == kAbortReasonWakeWordDetected) {
        message += ",\"reason\":\"wake_word_detected\"";
    }
    message += "}";
    SendText(message);
}

void Protocol::SendWakeWordDetected(const std::string& wake_word) {
    std::string json = "{\"session_id\":\"" + session_id_ +
                       "\",\"type\":\"listen\",\"state\":\"detect\",\"text\":\"" + wake_word +
                       "\"}";
    SendText(json);
}

// 按自动、手动、实时模式生成标准 listen 消息；实时模式需配合实际 AEC 能力。
void Protocol::SendStartListening(ListeningMode mode) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\"";
    message += ",\"type\":\"listen\",\"state\":\"start\"";
    if (mode == kListeningModeRealtime) {
        message += ",\"mode\":\"realtime\"";
    } else if (mode == kListeningModeAutoStop) {
        message += ",\"mode\":\"auto\"";
    } else {
        message += ",\"mode\":\"manual\"";
    }
    message += "}";
    SendText(message);
}

void Protocol::SendStopListening() {
    std::string message =
        "{\"session_id\":\"" + session_id_ + "\",\"type\":\"listen\",\"state\":\"stop\"}";
    SendText(message);
}

// 将已序列化的 MCP JSON 作为对象嵌入 payload，不是把它再次转成转义字符串。
void Protocol::SendMcpMessage(const std::string& payload) {
    std::string message =
        "{\"session_id\":\"" + session_id_ + "\",\"type\":\"mcp\",\"payload\":" + payload + "}";
    SendText(message);
}

// 通用传输的收包超时为 120 秒，与 ACOS 对话无输入 60 秒回待命不是同一计时器。
bool Protocol::IsTimeout() const {
    const int kTimeoutSeconds = 120;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_incoming_time_);
    bool timeout = duration.count() > kTimeoutSeconds;
    if (timeout) {
        ESP_LOGE(TAG, "Channel timeout %ld seconds", (long)duration.count());
    }
    return timeout;
}

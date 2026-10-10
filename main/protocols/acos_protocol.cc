#include "acos_protocol.h"

#if CONFIG_USE_ACOS
#include <esp_log.h>
#include <esp_pthread.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/base64.h>
#include <web_socket.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>
#include "acos_local_config.h"
#include "assets/lang_config.h"
#include "board.h"
#include "settings.h"

namespace {
constexpr const char* TAG = "ACOS";
constexpr size_t kMaxMessage = 16384;
// TTS 可能比实际播放更快地产生数据；60ms 音频最多缓存约两分钟。
// 同时限制总字节数，避免长回答或异常消息耗尽 PSRAM。
constexpr size_t kMaxReceiveBytes = 512 * 1024;
constexpr size_t kMaxReceiveFrames = 2048;
constexpr size_t kMaxUploadFrames = 50;  // 最多一秒 20ms 音频，超时不补发旧语音。
std::string JsonString(cJSON* root) {
    char* data = cJSON_PrintUnformatted(root);
    std::string result = data ? data : "";
    cJSON_free(data);
    cJSON_Delete(root);
    return result;
}
const char* StringField(const cJSON* root, const char* key) {
    auto value = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(value) ? value->valuestring : "";
}
bool IsDone(const char* type) {
    return !strcmp(type, "response.audio.done") || !strcmp(type, "response.done") ||
           !strcmp(type, "response.cancelled") || !strcmp(type, "response.canceled");
}
}  // namespace

AcosProtocol::~AcosProtocol() {
    stopping_ = true;
    if (worker_.joinable())
        worker_.join();
}

bool AcosProtocol::Start() {
    Settings settings("acos", false);
    url_ = settings.GetString("url", ACOS_LOCAL_URL);
    token_ = settings.GetString("token", ACOS_LOCAL_TOKEN);
    server_sample_rate_ = 16000;
    server_frame_duration_ = 60;
    if (url_.rfind("wss://", 0) != 0 || token_.empty()) {
        ESP_LOGE(TAG, "Configure ACOS wss URL and token in .local/acos.json");
        SetError("ACOS 配置缺失，请配置服务器地址和 token");
        return false;
    }
    auto config = esp_pthread_get_default_config();
    config.thread_name = "acos_network";
    config.stack_size = 8192;
    config.prio = 5;
    config.pin_to_core = 0;
    esp_pthread_set_cfg(&config);
    worker_ = std::thread([this] { Worker(); });
    ESP_LOGI(TAG, "Background connection started; microphone waits for local wake word");
    return true;
}

bool AcosProtocol::OpenAudioChannel() {
    // 建连和鉴权在后台进行，主任务不等待 TLS。连接恢复前丢弃过期语音。
    awake_ = true;
    ++epoch_;
    if (on_audio_channel_opened_)
        on_audio_channel_opened_();
    ESP_LOGI(TAG, "Conversation awake, transport %s", ready_ ? "ready" : "connecting");
    return true;
}

bool AcosProtocol::IsAudioChannelOpened() const { return awake_.load(); }

void AcosProtocol::InterruptLocked() {
    if (flow_.Interrupt()) {
        // 一轮最多一次取消；发送前再核对编号，防止延迟取消误伤下一轮回答。
        if (cancels_.size() < 8)
            cancels_.push_back({flow_.current, generation_});
        else
            reconnect_ = true;
    }
    // 队列不保留被打断的回答；音频任务里的 PCM 由主任务 ResetDecoder 清理。
    incoming_.clear();
    incoming_bytes_ = 0;
    playback_buffer_ = {};
    playback_finished_ = false;
}

void AcosProtocol::CloseAudioChannel(bool send_goodbye) {
    (void)send_goodbye;
    awake_ = false;
    ++epoch_;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        InterruptLocked();
        outgoing_.clear();
        incoming_.clear();
        incoming_bytes_ = 0;
        playback_turn_ = 0;
    }
    if (on_audio_channel_closed_)
        on_audio_channel_closed_();
    ESP_LOGI(TAG, "Conversation asleep; websocket retained, microphone upload off");
}

bool AcosProtocol::SendText(const std::string& text) {
    if (text.empty() || text.size() > kMaxMessage)
        return false;
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (outgoing_.size() >= kMaxUploadFrames) {
        outgoing_.pop_front();
        ++upload_drops_;
    }
    outgoing_.push_back({text, esp_timer_get_time(), epoch_.load()});
    return true;
}

bool AcosProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!awake_ || !ready_)
        return true;  // 不让断线阻塞编码队列。
    if (!packet || packet->payload.empty() || packet->payload.size() > 4096)
        return false;
    std::string encoded(4 * ((packet->payload.size() + 2) / 3) + 1, '\0');
    size_t size = 0;
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(encoded.data()), encoded.size(),
                              &size, packet->payload.data(), packet->payload.size()) != 0)
        return false;
    encoded.resize(size);
    cJSON* root = cJSON_CreateObject();
    char event_id[48];
    snprintf(event_id, sizeof(event_id), "event_%llu_%06lx",
             static_cast<unsigned long long>(esp_timer_get_time() / 1000),
             static_cast<unsigned long>(esp_random() & 0xffffff));
    cJSON_AddStringToObject(root, "event_id", event_id);
    cJSON_AddStringToObject(root, "type", "input_audio_buffer.append");
    cJSON_AddStringToObject(root, "audio", encoded.c_str());
    cJSON_AddStringToObject(root, "encoding", "opus");
    return SendText(JsonString(root));
}

void AcosProtocol::SendWakeWordDetected(const std::string& wake_word) {
    ESP_LOGI(TAG, "Local wake: %s", wake_word.c_str());
}
void AcosProtocol::SendStartListening(ListeningMode mode) {
    (void)mode;  // ACOS 使用服务器 VAD，不发送小智 listen/hello 消息。
}
void AcosProtocol::SendStopListening() { CloseAudioChannel(); }
void AcosProtocol::SendMcpMessage(const std::string& message) {
    (void)message;  // ACOS 尚未声明小智 MCP 协议兼容性。
}
void AcosProtocol::SendAbortSpeaking(AbortReason reason) {
    (void)reason;
    std::lock_guard<std::mutex> lock(queue_mutex_);
    InterruptLocked();
    playback_turn_ = 0;
}

void AcosProtocol::Emit(const char* type, const char* state, const char* text) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", type);
    if (state)
        cJSON_AddStringToObject(root, "state", state);
    if (text)
        cJSON_AddStringToObject(root, "text", text);
    if (on_incoming_json_)
        on_incoming_json_(root);
    cJSON_Delete(root);
}

void AcosProtocol::Receive(const char* data, size_t size, bool binary) {
    last_receive_us_ = esp_timer_get_time();
    if (!data || size == 0 || size > kMaxMessage) {
        reconnect_ = true;
        return;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        binary ? nullptr : cJSON_ParseWithLength(data, size), cJSON_Delete);
    const char* type = StringField(root.get(), "type");
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (!binary && !strcmp(type, "__proxy_connected__")) {
        ready_ = true;
        connection_changed_ = true;
        ESP_LOGI(TAG, "Websocket authenticated; local wake gate retained");
        return;
    }
    if (!ready_)
        return;
    if (!binary && (!strcmp(type, "input_audio_buffer.speech_started") ||
                    !strcmp(type, "input_audio_buffer.speech_stopped") || IsDone(type))) {
        ESP_LOGI(TAG, "Event %s: turn=%lu open=%d speech_gate=%d", type,
                 static_cast<unsigned long>(flow_.current), flow_.open,
                 flow_.waiting_for_speech_end);
    }
    if (!binary && !strcmp(type, "error")) {
        auto details = cJSON_GetObjectItemCaseSensitive(root.get(), "error");
        if (!cJSON_IsObject(details))
            details = root.get();
        if (!strcmp(StringField(details, "code"), "response_cancel_not_active") ||
            !strcmp(StringField(details, "message"), "Conversation has none active response")) {
            if (!flow_.Accepts(flow_.current))
                flow_.Done();
            ESP_LOGI(TAG, "Cancel acknowledged: response already inactive");
            return;
        }
        ESP_LOGW(TAG, "Server returned an error (payload omitted)");
        return;
    }
    if (!awake_) {
        if (!binary && IsDone(type))
            flow_.Done();
        return;
    }
    if (!binary && !strcmp(type, "input_audio_buffer.speech_started")) {
        // 高优先级处理新讲话；不等待已经排队的旧回答播放完。
        InterruptLocked();
        interrupted_ = true;
        return;
    }
    // speech_stopped 只说明用户说完了，不代表旧回答已停止发送。
    // 只有回答结束/取消确认才解除迟到音频屏障。
    if (!binary && !strcmp(type, "input_audio_buffer.speech_stopped"))
        flow_.SpeechStopped();
    uint32_t turn = 0;
    if (binary) {
        const auto now = esp_timer_get_time();
        if (last_audio_us_ && flow_.open)
            max_audio_gap_us_ = std::max(max_audio_gap_us_, now - last_audio_us_);
        last_audio_us_ = now;
        ++rx_audio_count_;
        turn = flow_.Audio();
        if (!flow_.Accepts(turn)) {
            ++stale_audio_count_;
            return;
        }
    } else if (IsDone(type))
        turn = flow_.Done();
    else if (!strcmp(type, "response.text.delta") ||
             !strcmp(type, "response.audio_transcript.delta")) {
        turn = flow_.current;
    }
    if (turn && !flow_.Accepts(turn))
        return;
    if (incoming_.size() >= kMaxReceiveFrames || incoming_bytes_ + size > kMaxReceiveBytes) {
        ESP_LOGW(TAG, "Receive queue full; reconnecting instead of playing partial/stale audio");
        reconnect_ = true;
        return;
    }
    Frame frame;
    frame.data.reset(
        static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!frame.data) {
        reconnect_ = true;
        return;
    }
    memcpy(frame.data.get(), data, size);
    frame.size = size;
    frame.binary = binary;
    frame.done = !binary && IsDone(type);
    frame.turn = turn;
    incoming_bytes_ += size;
    incoming_.push_back(std::move(frame));
}

void AcosProtocol::Poll() {
    // 只由主任务调用。每轮最多一帧，状态变更先于下一帧音频处理。
    Frame frame;
    bool interrupted = false;
    bool changed = false;
    bool start = false;
    const auto now = esp_timer_get_time();
    bool log_stats = false;
    size_t rx_queued = 0, tx_queued = 0;
    uint32_t received = 0, stale = 0, drops = 0, underruns = 0;
    int64_t gap = 0;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (awake_ && now - last_stats_us_ >= 10000000) {
            last_stats_us_ = now;
            log_stats = true;
            rx_queued = incoming_.size();
            tx_queued = outgoing_.size();
            received = rx_audio_count_;
            stale = stale_audio_count_;
            drops = upload_drops_;
            underruns = underruns_;
            gap = std::exchange(max_audio_gap_us_, 0);
        }
    }
    if (log_stats)
        ESP_LOGI(TAG,
                 "Audio stats: rx=%lu queued=%u tx_queue=%u dropped_tx=%lu stale_rx=%lu "
                 "underruns=%lu max_rx_gap=%lldms",
                 (unsigned long)received, (unsigned)rx_queued, (unsigned)tx_queued,
                 (unsigned long)drops, (unsigned long)stale, (unsigned long)underruns, gap / 1000);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        changed = std::exchange(connection_changed_, false);
        interrupted = std::exchange(interrupted_, false);
        if (interrupted) {
            playback_turn_ = 0;
            playback_buffer_ = {};
            playback_finished_ = false;
        }
        if (!changed && !interrupted) {
            while (!incoming_.empty() && incoming_.front().turn &&
                   !flow_.Accepts(incoming_.front().turn)) {
                incoming_bytes_ -= incoming_.front().size;
                incoming_.pop_front();
            }
            if (!incoming_.empty()) {
                auto& next = incoming_.front();
                if (next.binary && next.turn != playback_turn_) {
                    playback_turn_ = next.turn;
                    playback_buffer_ = {};
                    playback_finished_ = false;
                    start = true;
                } else if (next.binary && playback_ready_ && !playback_ready_()) {
                    return;  // 解码队列满时保留原帧；不在网络/主任务中等待。
                } else {
                    if (next.binary) {
                        if (!playback_finished_ && playback_idle_ &&
                            playback_buffer_.Underrun(playback_idle_())) {
                            ++underruns_;
                            ESP_LOGW(TAG, "Playback underrun: turn=%lu, rebuffering 300ms",
                                     (unsigned long)playback_turn_);
                        }
                        if (playback_buffer_.buffering) {
                            unsigned queued_ms = 0;
                            unsigned inspected = 0;
                            bool finished = false;
                            for (const auto& queued : incoming_) {
                                if (++inspected > 64)
                                    break;
                                if (queued.turn != playback_turn_)
                                    continue;
                                if (queued.binary)
                                    queued_ms += server_frame_duration_;
                                else
                                    finished = queued.done;
                                if (queued_ms >= 300 || finished)
                                    break;
                            }
                            if (!playback_buffer_.Ready(queued_ms, finished, now))
                                return;
                            ESP_LOGI(TAG, "Playback buffered: turn=%lu queued=%ums finished=%d",
                                     (unsigned long)playback_turn_, queued_ms, finished);
                        }
                        playback_buffer_.delivered = true;
                    }
                    frame = std::move(next);
                    incoming_bytes_ -= frame.size;
                    incoming_.pop_front();
                }
            }
        }
    }
    if (changed) {
        Emit(ready_ ? "acos.connected" : "acos.disconnected");
        if (interrupted && awake_)
            Emit("acos.speech_started");
        return;
    }
    if (interrupted) {
        Emit("acos.speech_started");
        return;
    }
    if (start) {
        Emit("acos.audio.start");
        return;
    }
    if (!frame.data)
        return;
    if (frame.binary) {
        auto packet = std::make_unique<AudioStreamPacket>();
        packet->sample_rate = server_sample_rate_;
        packet->frame_duration = server_frame_duration_;
        packet->payload.assign(frame.data.get(), frame.data.get() + frame.size);
        if (on_incoming_audio_)
            on_incoming_audio_(std::move(packet));
        return;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLength(reinterpret_cast<char*>(frame.data.get()), frame.size), cJSON_Delete);
    auto type = StringField(root.get(), "type");
    if (IsDone(type)) {
        bool finished = false;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            finished = frame.turn == playback_turn_ && flow_.Accepts(frame.turn);
            if (finished)
                playback_finished_ = true;
        }
        if (finished) {
            Emit("acos.audio.done");
        }
    } else if (!strcmp(type, "input_audio_buffer.speech_stopped")) {
        Emit("acos.speech_stopped");
    } else if (!strcmp(type, "response.audio_transcript.delta") ||
               !strcmp(type, "response.text.delta")) {
        Emit("tts", "sentence_start", StringField(root.get(), "delta"));
    } else if (!strcmp(type, "conversation.item.input_audio_transcription.completed")) {
        Emit("stt", nullptr, StringField(root.get(), "transcript"));
    }
}

void AcosProtocol::Worker() {
    while (!stopping_) {
        reconnect_ = false;
        auto websocket = Board::GetInstance().GetNetwork()->CreateWebSocket(2);
        if (!websocket)
            break;
        websocket->SetReceiveBufferSize(4096);
        websocket->OnData(
            [this](const char* data, size_t size, bool binary) { Receive(data, size, binary); });
        websocket->OnDisconnected([this] { reconnect_ = true; });
        websocket->OnError([this](const auto&) { reconnect_ = true; });
        websocket->OnPong([this](const char*, size_t) { last_receive_us_ = esp_timer_get_time(); });
        if (websocket->Connect(url_.c_str()) && !stopping_) {
            cJSON* auth = cJSON_CreateObject();
            cJSON_AddStringToObject(auth, "type", "auth");
            cJSON_AddStringToObject(auth, "token", token_.c_str());
            if (!websocket->Send(JsonString(auth)))
                reconnect_ = true;
            const auto auth_start = esp_timer_get_time();
            last_receive_us_ = auth_start;
            auto last_ping = auth_start;
            int64_t cancel_deadline = 0;
            uint32_t cancel_turn = 0;
            ESP_LOGI(TAG, "Websocket connected; authenticating");
            while (!stopping_ && !reconnect_) {
                const auto now = esp_timer_get_time();
                if (!ready_ && now - auth_start > 15000000)
                    break;
                if (now - last_receive_us_.load() > 60000000)
                    break;
                Upload upload;
                bool cancel = false;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    if (cancel_deadline && (!flow_.open || flow_.current != cancel_turn)) {
                        cancel_deadline = 0;
                    }
                    while (!cancels_.empty()) {
                        auto request = cancels_.front();
                        cancels_.pop_front();
                        if (request.generation == generation_ && request.turn == flow_.current &&
                            flow_.open) {
                            cancel = true;
                            cancel_turn = request.turn;
                            break;
                        }
                    }
                    if (!cancel && !outgoing_.empty()) {
                        upload = std::move(outgoing_.front());
                        outgoing_.pop_front();
                    }
                }
                if (cancel_deadline && now >= cancel_deadline) {
                    ESP_LOGW(TAG, "Cancellation acknowledgement timed out; reconnecting");
                    break;
                }
                if (cancel && ready_) {
                    if (!websocket->Send("{\"type\":\"response.cancel\"}"))
                        break;
                    cancel_deadline = esp_timer_get_time() + 10000000;
                    ESP_LOGI(TAG, "Response cancel sent; microphone stream retained");
                } else if (!upload.json.empty() && awake_ && ready_ && upload.epoch == epoch_ &&
                           now - upload.created_us < 1000000) {
                    if (!websocket->Send(upload.json))
                        break;
                } else {
                    if (!upload.json.empty()) {
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        ++upload_drops_;
                    }
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                if (now - last_ping > 20000000) {
                    websocket->Ping();
                    last_ping = now;
                }
            }
        }
        // 先停止回调，再清理帧及代数；不会把重连前的音频带入新会话。
        websocket.reset();
        ready_ = false;
        ++epoch_;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            incoming_.clear();
            outgoing_.clear();
            cancels_.clear();
            incoming_bytes_ = 0;
            flow_ = {};
            ++generation_;
            playback_turn_ = 0;
            playback_buffer_ = {};
            playback_finished_ = false;
            last_audio_us_ = 0;
            interrupted_ = false;
            connection_changed_ = true;
        }
        if (!stopping_)
            ESP_LOGW(TAG, "Transport unavailable; retry in 5 seconds");
        for (int i = 0; i < 50 && !stopping_; ++i)
            vTaskDelay(pdMS_TO_TICKS(100));
    }
}
#endif

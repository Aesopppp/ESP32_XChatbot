/*
 * ESP-VoCat 当前使用的 Emote 显示适配层。
 * 把应用的状态、字幕、表情请求转换为 expression_emote 事件，并对接 LCD 刷新完成回调。
 * Flash 资源映射由 Emote 管理；直接操作其 LVGL 控件时，应使用 emote_lock 而非本类的空锁。
 */

#include "emote_display.h"

// Standard C++ headers
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <tuple>
#include <unordered_map>

// Standard C headers
#include <sys/time.h>
#include <time.h>

// ESP-IDF headers
#include <esp_lcd_panel_io.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <lvgl.h>

// FreeRTOS headers
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Project headers
#include "assets.h"
#include "assets/lang_config.h"
#include "board.h"
#include "expression_emote.h"
#include "gfx.h"

namespace emote {

// ============================================================================
// Constants and Type Definitions
// ============================================================================

static const char* TAG = "EmoteDisplay";

// ============================================================================
// Forward Declarations
// ============================================================================

class EmoteDisplay;

// ============================================================================
// Helper Functions
// ============================================================================

static bool OnFlushIoReady(const esp_lcd_panel_io_handle_t panel_io,
                           esp_lcd_panel_io_event_data_t* const edata, void* user_ctx) {
    emote_handle_t handle = static_cast<emote_handle_t>(user_ctx);
    if (handle) {
        emote_notify_flush_finished(handle);
    }
    return true;
}

// Flush callback for emote
static void OnFlushCallback(int x_start, int y_start, int x_end, int y_end, const void* data,
                            emote_handle_t handle) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)emote_get_user_data(handle);
    if (panel != nullptr) {
        esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, data);
    }
}

// ============================================================================
// Graphics Initialization Functions
// ============================================================================

static emote_handle_t InitializeEmote(const esp_lcd_panel_handle_t panel, const int width,
                                      const int height) {
    if (!panel) {
        ESP_LOGE(TAG, "Invalid panel");
        return nullptr;
    }

    emote_config_t emote_cfg = {
        .flags =
            {
                .swap = true,
                .double_buffer = true,
                .buff_dma = false,
            },
        .gfx_emote =
            {
                .h_res = width,
                .v_res = height,
                .fps = 30,
            },
        .buffers =
            {
                .buf_pixels = static_cast<size_t>(width * 16),
            },
        .task =
            {
                .task_priority = 5,
                .task_stack = 6 * 1024,
                .task_affinity = 0,
                .task_stack_in_ext = false,
            },
        .flush_cb = OnFlushCallback,
        .user_data = (void*)panel,
    };

    emote_handle_t emote_handle = emote_init(&emote_cfg);
    if (!emote_handle) {
        ESP_LOGE(TAG, "Failed to initialize emote");
        return nullptr;
    }

    return emote_handle;
}

// ============================================================================
// EmoteDisplay Class Implementation
// ============================================================================

EmoteDisplay::EmoteDisplay(const esp_lcd_panel_handle_t panel,
                           const esp_lcd_panel_io_handle_t panel_io, const int width,
                           const int height) {
    emote_handle_ = InitializeEmote(panel, width, height);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = OnFlushIoReady,
    };
    esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, emote_handle_);
}

EmoteDisplay::~EmoteDisplay() {
    if (emote_handle_) {
        emote_deinit(emote_handle_);
        emote_handle_ = nullptr;
    }
}

// 根据表情资源名称切动画；不负责更改应用的对话状态。
void EmoteDisplay::SetEmotion(const char* const emotion) {
    ESP_LOGI(TAG, "SetEmotion: %s", emotion);
    if (emote_handle_ && emotion && strlen(emotion) > 0) {
        emote_set_anim_emoji(emote_handle_, emotion);
    }
}

// 将系统提示和正常字幕分发到不同 Emote 事件。
// 配网链接的换行先转换为空格，便于在设备提示框中显示。
void EmoteDisplay::SetChatMessage(const char* const role, const char* const content) {
    ESP_LOGI(TAG, "SetChatMessage: %s, %s", role, content);
    if (emote_handle_ && content && strlen(content) > 0) {
        if ((std::strcmp(role, "system") == 0) && std::strstr(content, "xiaozhi.me")) {
            size_t len = strlen(content);
            char* new_content = new char[len + 1];
            strcpy(new_content, content);
            std::replace(new_content, new_content + len, static_cast<char>(0x0A),
                         static_cast<char>(0x20));
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SYS, new_content);
            delete[] new_content;
        } else {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SPEAK, content);
        }
    }
}

// 应用状态文本映射到 Emote 事件，最终图标由资源包定义。
// 更换播放图标需重打包图像资源，仅修改状态文字不会改变图标像素。
// 把本地化状态文字映射为聆听、待命、说话等动画事件。
// 此处只更新呈现；不启动或停止音频任务。
void EmoteDisplay::SetStatus(const char* const status) {
    ESP_LOGI(TAG, "SetStatus: %s", status);
    if (emote_handle_ && status && strlen(status) > 0) {
        if (std::strcmp(status, Lang::Strings::LISTENING) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_LISTEN, NULL);
        } else if (std::strcmp(status, Lang::Strings::STANDBY) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_IDLE, NULL);
        } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SPEAK, NULL);
        } else if (std::strcmp(status, Lang::Strings::ERROR) == 0) {
            emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SET, NULL);
        }
    }
}

// 把通知交给 Emote 系统提示事件；此实现未使用 duration_ms，显示时长由 Emote 决定。
void EmoteDisplay::ShowNotification(const char* notification, int duration_ms) {
    ESP_LOGI(TAG, "ShowNotification: %s", notification);
    if (emote_handle_ && notification && strlen(notification) > 0) {
        emote_set_event_msg(emote_handle_, EMOTE_MGR_EVT_SYS, notification);
    }
}

// 当前为状态栏扩展入口，仅检查句柄和记录调用，并未更新电池或网络图标。
void EmoteDisplay::UpdateStatusBar(bool update_all) {
    ESP_LOGD(TAG, "UpdateStatusBar: %s", update_all ? "true" : "false");
    if (!emote_handle_) {
        return;
    }
}

// 当前仅保留省电接口和日志，实际音频 / 网络省电由各自模块控制。
void EmoteDisplay::SetPowerSaveMode(bool on) {
    ESP_LOGI(TAG, "SetPowerSaveMode: %s", on ? "ON" : "OFF");
    if (!emote_handle_) {
        return;
    }
}

// 本后端暂不提供通用图片预览，不能把调用成功等同于图片已经显示。
void EmoteDisplay::SetPreviewImage(const void* image) {
    if (image) {
        ESP_LOGI(TAG, "SetPreviewImage: Preview image not supported, using default icon");
    }
}

// 从 Flash 分区 mmap 挂载表情资源；资源数据指针只在保持挂载期间有效。
bool EmoteDisplay::MountAssets(const char* partition_label) {
    if (emote_handle_ == nullptr || partition_label == nullptr) {
        return false;
    }
    const emote_data_t data = {
        .type = EMOTE_SOURCE_PARTITION,
        .source =
            {
                .partition_label = partition_label,
            },
        .flags =
            {
                .mmap_enable = true,
            },
    };
    return emote_mount_assets(emote_handle_, &data) == ESP_OK;
}

// 卸载映射后原资源指针失效，应先停止使用对应资源的动画 / 控件。
void EmoteDisplay::UnmountAssets() {
    if (emote_handle_ != nullptr) {
        emote_unmount_assets(emote_handle_);
    }
}

// 返回资源的借用指针与长度；资源仍属于 Emote，调用方不能释放该指针。
bool EmoteDisplay::GetAssetData(const std::string& name, const uint8_t*& data, size_t& size) {
    if (emote_handle_ == nullptr) {
        return false;
    }
    return emote_get_asset_data_by_name(emote_handle_, name.c_str(), &data, &size) == ESP_OK;
}

// 加载已挂载资源中的动画配置，挂载与加载是两个不同步骤。
void EmoteDisplay::LoadAssets() {
    if (emote_handle_ != nullptr) {
        emote_load_assets(emote_handle_);
    }
}

void EmoteDisplay::SetTheme(Theme* const theme) { ESP_LOGI(TAG, "SetTheme: %p", theme); }

// 此接口返回成功但不真正加锁；直接改 Emote 内部控件需使用 emote_lock。
bool EmoteDisplay::Lock(const int timeout_ms) {
    (void)timeout_ms;
    return true;
}

void EmoteDisplay::Unlock() {}

// 停止临时插入的动画对话，恢复正常的状态 / 表情显示流程。
bool EmoteDisplay::StopAnimDialog() {
    ESP_LOGI(TAG, "StopAnimDialog");
    if (emote_handle_) {
        return emote_stop_anim_dialog(emote_handle_);
    }
    return false;
}

// 按名称插入定时动画，时长单位为毫秒；不改变应用的对话状态。
bool EmoteDisplay::InsertAnimDialog(const char* emoji_name, uint32_t duration_ms) {
    ESP_LOGI(TAG, "InsertAnimDialog: %s, %" PRIu32, emoji_name, duration_ms);
    if (emote_handle_ && emoji_name) {
        return emote_insert_anim_dialog(emote_handle_, emoji_name, duration_ms);
    }
    return false;
}

// 请求 Emote 重新绘制界面，用于菜单退出等场景下恢复画面。
void EmoteDisplay::RefreshAll() {
    if (emote_handle_) {
        emote_notify_all_refresh(emote_handle_);
        return;
    }
}

}  // namespace emote

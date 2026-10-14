/*
 * 显示抽象接口：让应用通过统一方法更新状态、表情、字幕及资源。
 * 屏幕只呈现应用状态；真正的录音、播放和联网状态由 Application / AudioService / Protocol 控制。
 * 部分硬件没有屏幕或不支持某项能力，基类提供空操作；锁的实现由具体显示后端决定。
 */

#include "display.h"
#include <esp_err.h>
#include <esp_log.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include "application.h"
#include "assets/lang_config.h"
#include "audio_codec.h"
#include "board.h"
#include "lvgl_display/lvgl_image.h"
#include "settings.h"

#define TAG "Display"

Display::Display() {}

Display::~Display() {}

void Display::SetPreviewImage(std::unique_ptr<LvglImage> image) {}

void Display::SetStatus(const char* status) { ESP_LOGW(TAG, "SetStatus: %s", status); }

void Display::ShowNotification(const std::string& notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void Display::ShowNotification(const char* notification, int duration_ms) {
    ESP_LOGW(TAG, "ShowNotification: %s", notification);
}

void Display::UpdateStatusBar(bool update_all) {}

void Display::SetEmotion(const char* emotion) { ESP_LOGW(TAG, "SetEmotion: %s", emotion); }

void Display::SetChatMessage(const char* role, const char* content) {
    ESP_LOGW(TAG, "Role:%s", role);
    ESP_LOGW(TAG, "     %s", content);
}

// 基类通过空系统消息表达清屏；有聊天列表的子类可实现完整列表清理。
void Display::ClearChatMessages() {
    // Default empty implementation, override in subclasses if needed
}

// 保存当前主题引用及 NVS 中的主题名称，主题对象本身由调用方管理。
void Display::SetTheme(Theme* theme) {
    current_theme_ = theme;
    Settings settings("display", true);
    settings.SetString("theme", theme->name());
}

void Display::SetPowerSaveMode(bool on) { ESP_LOGW(TAG, "SetPowerSaveMode: %d", on); }

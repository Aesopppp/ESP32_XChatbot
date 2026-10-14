/*
 * ESP-VoCat 当前使用的 Emote 显示适配层。
 * 把应用的状态、字幕、表情请求转换为 expression_emote 事件，并对接 LCD 刷新完成回调。
 * Flash 资源映射由 Emote 管理；直接操作其 LVGL 控件时，应使用 emote_lock 而非本类的空锁。
 */

#pragma once

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <memory>
#include <string>
#include "display.h"
#include "expression_emote.h"

namespace emote {

class EmoteDisplay : public Display {
public:
    using Display::SetPreviewImage;

    EmoteDisplay(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t panel_io, int width,
                 int height);
    virtual ~EmoteDisplay();

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetTheme(Theme* theme) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void UpdateStatusBar(bool update_all = false) override;
    virtual void SetPowerSaveMode(bool on) override;
    virtual void SetPreviewImage(const void* image);

    bool StopAnimDialog();
    virtual bool InsertAnimDialog(const char* emoji_name, uint32_t duration_ms) override;
    virtual bool MountAssets(const char* partition_label) override;
    virtual void UnmountAssets() override;
    virtual bool GetAssetData(const std::string& name, const uint8_t*& data, size_t& size) override;
    virtual void LoadAssets() override;

    void RefreshAll();

    // Get emote handle for internal use
    // 提供给 VoCat 菜单等受控扩展使用的借用句柄；访问内部控件时需获取 Emote 锁。
    emote_handle_t GetEmoteHandle() const { return emote_handle_; }

private:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    // 本类持有的 Emote 实例，析构时释放；外部不能单独销毁。
    emote_handle_t emote_handle_ = nullptr;
};

}  // namespace emote

/*
 * LVGL 显示公共层：通知、状态栏、动态字形、字体切换和截图。
 * 具体屏幕子类创建控件并实现 Lock / Unlock；跨任务访问控件时需要显示锁。
 * 空指针通常表示控件尚未建立或该屏幕没有对应能力，不能假定所有显示设备功能相同。
 */

#ifndef LVGL_DISPLAY_H
#define LVGL_DISPLAY_H

#include "display.h"
#include "lvgl_image.h"

#include <esp_log.h>
#include <esp_pm.h>
#include <esp_timer.h>
#include <lvgl.h>

#include <chrono>
#include <memory>
#include <string>

class DynamicGlyphCache;
class LvglFont;

class LvglDisplay : public Display {
public:
    LvglDisplay();
    virtual ~LvglDisplay();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string& notification, int duration_ms = 3000);
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);
    virtual bool SnapshotToJpeg(std::string& jpeg_data, int quality = 80) override;
    virtual bool AddTextGlyphs(const std::vector<TextGlyph>& glyphs, uint8_t bpp) override;
    virtual void ClearTextGlyphs() override;
    virtual bool SetTextFont(std::shared_ptr<LvglFont> text_font) override;
    virtual bool SupportsGuiOperations() const override { return true; }

protected:
    // 截图等临时操作可提高总线工作频率，完成后释放锁恢复原来的电源策略。
    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t* display_ = nullptr;

    // 这些控件由具体屏幕的 SetupUI 创建，公共层只维护显示内容；nullptr 表示尚不可用。
    lv_obj_t* network_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* notification_label_ = nullptr;
    lv_obj_t* mute_label_ = nullptr;
    lv_obj_t* battery_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;

    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;

    std::chrono::system_clock::time_point last_status_update_time_;
    // 通知超时在 ESP 定时器任务中执行，不在应用主事件循环中执行。
    esp_timer_handle_t notification_timer_ = nullptr;
    int last_displayed_clock_min_ = -1;   // -1 forces update on first idle tick
    // 独占动态字形缓存，字体 fallback 的寿命与显示实例关联。
    std::unique_ptr<DynamicGlyphCache> dynamic_glyph_cache_;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;
};

#endif

#pragma once

#include "sdkconfig.h"
#if CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <lvgl.h>
#include <array>
#include <functional>
#include <mutex>
#include <string>

#include "emote_display.h"
#include "vocat_touch_policy.h"

// 菜单与表情共用同一个 GFX 绘制任务/flush 回调，不另开 LVGL 屏幕驱动。
// 所有页面和硬件控制操作由 Application 主循环调用。
class VocatSettingsDisplay : public emote::EmoteDisplay {
public:
    using emote::EmoteDisplay::EmoteDisplay;
    ~VocatSettingsDisplay() override;
    void SetWifiActions(std::function<bool()> enter, std::function<void()> leave);
    bool HandleTouch(const vocat::TouchResult& touch);
    void ShowProvisioning();
    void UpdateStatusBar(bool update_all = false) override;
    void LoadAssets() override;
    void UnmountAssets() override;

private:
    enum class Page { Face, Settings, ConfirmWifi, Provisioning };
    enum Item {
        Background,
        Title,
        VolumeMinus,
        Volume,
        VolumePlus,
        BrightnessMinus,
        Brightness,
        BrightnessPlus,
        Info,
        Wifi,
        Back,
        Hint,
        QrBackground,
        Qr,
        Count
    };
    Page page_ = Page::Face;
    bool created_ = false;
    // Assets::Apply 在后台激活任务中执行，与主循环的菜单访问互斥。
    std::recursive_mutex menu_mutex_;
    int64_t provisioning_started_us_ = 0;
    bool saw_config_ap_ = false;
    bool leaving_config_ = false;
    std::array<gfx_obj_t*, Count> items_{};
    // 按钮底色独立于文字，保留完整点击区域，同时允许文字垂直居中。
    std::array<gfx_obj_t*, Count> button_backgrounds_{};
    std::array<std::string, Count> text_{};
    std::string qr_data_;
    lv_font_t* status_font_ = nullptr;
    std::function<bool()> enter_wifi_;
    std::function<void()> leave_wifi_;

    bool EnsureObjects();
    void ResetObjects();
    void SetPage(Page page);
    void Render();
    void Label(Item item, int x, int y, int w, int h, const std::string& text, bool button = false);
};
#endif

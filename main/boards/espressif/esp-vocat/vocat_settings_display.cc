#include "vocat_settings_display.h"

#if CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <esp_timer.h>
#include <algorithm>
#include <cstdio>
#include <utility>

#include "audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "gfx.h"
#include "lvgl.h"
#include "widget/gfx_font_lvgl.h"
#include "wifi_manager.h"

extern "C" const lv_font_t vocat_menu_font_20;

VocatSettingsDisplay::~VocatSettingsDisplay() { UnmountAssets(); }

void VocatSettingsDisplay::SetWifiActions(std::function<bool()> enter,
                                          std::function<void()> leave) {
    enter_wifi_ = std::move(enter);
    leave_wifi_ = std::move(leave);
}

void VocatSettingsDisplay::LoadAssets() {
    std::lock_guard<std::recursive_mutex> lock(menu_mutex_);
    // 首次配网页面可在表情资源加载之前创建。先销毁旧对象，再将菜单建在
    // 新表情之后，保证覆盖顺序正确；字库编入固件，不依赖联网或 SD 卡。
    emote_unload_assets(GetEmoteHandle());
    ResetObjects();
    emote::EmoteDisplay::LoadAssets();
    // 联网后可显示任意常用中文 SSID；配网热点名为 ASCII，不依赖此资源。
    const uint8_t* data = nullptr;
    size_t size = 0;
    if (GetAssetData("font_puhui_common_20_4.bin", data, size) && size > sizeof(lv_font_t))
        status_font_ = gfx_font_lv_load_from_binary(const_cast<uint8_t*>(data));
}

void VocatSettingsDisplay::ResetObjects() {
    if (status_font_) {
        gfx_font_lv_delete(status_font_);
        status_font_ = nullptr;
    }
    created_ = false;
    items_.fill(nullptr);
    text_.fill("");
    qr_data_.clear();
    page_ = Page::Face;
}

void VocatSettingsDisplay::UnmountAssets() {
    std::lock_guard<std::recursive_mutex> lock(menu_mutex_);
    // 必须先删除引用 mmap 数据的对象，再解除资源映射。
    emote_unload_assets(GetEmoteHandle());
    ResetObjects();
    emote::EmoteDisplay::UnmountAssets();
}

bool VocatSettingsDisplay::EnsureObjects() {
    if (created_)
        return true;
    if (!GetEmoteHandle())
        return false;
    auto handle = GetEmoteHandle();
    // 官方接口按创建顺序叠加对象。资源加载完成后创建菜单，完整背景遮住表情。
    for (int i = 0; i < Count; ++i) {
        if (items_[i])
            continue;
        char name[32];
        snprintf(name, sizeof(name), "vocat_settings_%d", i);
        items_[i] = emote_create_obj_by_type(handle, i == Qr ? "qrcode" : "label", name);
        if (!items_[i]) {
            ESP_LOGE("VocatMenu", "Cannot allocate menu object %d", i);
            return false;
        }
        emote_lock(handle);
        gfx_obj_set_visible(items_[i], false);
        if (i != Qr) {
            gfx_label_set_font(items_[i], i == Info && status_font_
                                              ? status_font_
                                              : const_cast<lv_font_t*>(&vocat_menu_font_20));
            gfx_label_set_text_align(items_[i], GFX_TEXT_ALIGN_CENTER);
            gfx_label_set_long_mode(items_[i], GFX_LABEL_LONG_CLIP);
            gfx_label_set_color(items_[i], GFX_COLOR_HEX(0xF2F6FC));
        } else {
            gfx_qrcode_set_color(items_[i], GFX_COLOR_HEX(0x000000));
            gfx_qrcode_set_bg_color(items_[i], GFX_COLOR_HEX(0xFFFFFF));
            gfx_qrcode_set_size(items_[i], 128);
        }
        emote_unlock(handle);
    }
    created_ = true;
    return true;
}

void VocatSettingsDisplay::Label(Item item, int x, int y, int w, int h, const std::string& text,
                                 bool button) {
    auto obj = items_[item];
    gfx_obj_set_pos(obj, x, y);
    gfx_obj_set_size(obj, w, h);
    gfx_label_set_bg_enable(obj, button || item == Background);
    gfx_label_set_bg_color(obj, GFX_COLOR_HEX(button ? 0x244864 : 0x101B2A));
    if (text_[item] != text) {
        text_[item] = text;
        gfx_label_set_text(obj, text.c_str());
    }
    gfx_obj_set_visible(obj, true);
}

void VocatSettingsDisplay::SetPage(Page page) {
    if (page != Page::Face && !EnsureObjects()) {
        ShowNotification("菜单暂不可用，请稍后重试");
        return;
    }
    page_ = page;
    Render();
    RefreshAll();
}

void VocatSettingsDisplay::Render() {
    if (!created_)
        return;
    // 网络/codec 查询不持有绘图锁，避免跨服务锁顺序反转。
    auto& wifi = WifiManager::GetInstance();
    auto& board = Board::GetInstance();
    const int volume = board.GetAudioCodec()->output_volume();
    auto backlight = board.GetBacklight();
    const int brightness = backlight ? backlight->brightness() : 0;
    const bool ap = wifi.IsConfigMode();
    std::string status =
        wifi.IsConnected() ? wifi.GetSsid() + "\n" + wifi.GetIpAddress() : "Wi-Fi 未连接";
    std::string ssid = ap ? wifi.GetApSsid() : "";
    std::string url = ap ? wifi.GetApWebUrl() : "";
    auto handle = GetEmoteHandle();
    if (emote_lock(handle) != ESP_OK)
        return;
    for (auto obj : items_)
        gfx_obj_set_visible(obj, false);
    if (page_ == Page::Face) {
        emote_unlock(handle);
        return;
    }
    Label(Background, 0, 0, 360, 360, " ");
    if (page_ == Page::Settings) {
        Label(Title, 100, 30, 160, 30, "设备设置");
        Label(VolumeMinus, 48, 88, 50, 42, "-", true);
        Label(Volume, 103, 96, 154, 30, "音量  " + std::to_string(volume));
        Label(VolumePlus, 262, 88, 50, 42, "+", true);
        Label(BrightnessMinus, 48, 144, 50, 42, "-", true);
        Label(Brightness, 103, 152, 154, 30, "亮度  " + std::to_string(brightness));
        Label(BrightnessPlus, 262, 144, 50, 42, "+", true);
        Label(Info, 50, 196, 260, 50, status);
        Label(Wifi, 85, 253, 190, 36, "重新配网", true);
        Label(Back, 120, 302, 120, 32, "返回", true);
    } else if (page_ == Page::ConfirmWifi) {
        Label(Title, 80, 45, 200, 30, "重新配网？");
        Label(Info, 50, 106, 260, 78, "将结束当前对话\n保留原网络和账号\n连接设备热点进行设置");
        Label(Wifi, 90, 205, 180, 40, "确认配网", true);
        Label(Back, 110, 266, 140, 38, "取消", true);
    } else {
        Label(Title, 90, 28, 180, 30, "Wi-Fi 配网");
        if (ap && !leaving_config_) {
            Label(Info, 40, 60, 280, 28, ssid);
            // 二维码外保留白色静区，提高手机相机识别率。
            Label(QrBackground, 100, 92, 160, 160, " ", true);
            gfx_label_set_bg_color(items_[QrBackground], GFX_COLOR_HEX(0xFFFFFF));
            // 开放热点，二维码只包含公开 SSID，不包含 ACOS 或家庭 Wi-Fi 凭据。
            const std::string data = "WIFI:T:nopass;S:" + ssid + ";;";
            if (qr_data_ != data) {
                gfx_qrcode_set_data(items_[Qr], data.c_str());
                qr_data_ = data;
            }
            gfx_obj_set_pos(items_[Qr], 116, 108);
            gfx_obj_set_visible(items_[Qr], true);
            Label(Hint, 40, 256, 280, 50, "扫码连接热点，再访问\n" + url);
            Label(Back, 110, 309, 140, 32, "取消配网", true);
        } else {
            Label(Info, 50, 133, 260, 75,
                  leaving_config_ ? "正在恢复网络..." : "正在启动热点...\n请稍候");
        }
    }
    emote_unlock(handle);
}

void VocatSettingsDisplay::ShowProvisioning() {
    std::lock_guard<std::recursive_mutex> lock(menu_mutex_);
    if (page_ == Page::Provisioning)
        return;
    saw_config_ap_ = false;
    leaving_config_ = false;
    provisioning_started_us_ = esp_timer_get_time();
    SetPage(Page::Provisioning);
}

void VocatSettingsDisplay::UpdateStatusBar(bool update_all) {
    std::lock_guard<std::recursive_mutex> lock(menu_mutex_);
    emote::EmoteDisplay::UpdateStatusBar(update_all);
    const bool ap = WifiManager::GetInstance().IsConfigMode();
    if (ap && page_ != Page::Provisioning) {
        ShowProvisioning();
    }
    if (page_ == Page::Provisioning) {
        if (ap)
            saw_config_ap_ = true;
        else if (saw_config_ap_) {
            leaving_config_ = false;
            SetPage(Page::Face);
            return;
        } else if (esp_timer_get_time() - provisioning_started_us_ > 20000000) {
            // 若请求因升级/激活状态切换未被接受，不让界面永久停在等待页。
            SetPage(Page::Settings);
            ShowNotification("热点尚未启动，请稍后重试");
            return;
        }
    }
    if (page_ != Page::Face)
        Render();
}

bool VocatSettingsDisplay::HandleTouch(const vocat::TouchResult& touch) {
    std::lock_guard<std::recursive_mutex> lock(menu_mutex_);
    using vocat::Gesture;
    if (page_ == Page::Face) {
        if (touch.gesture == Gesture::SwipeUp) {
            SetPage(Page::Settings);
            return true;
        }
        return touch.gesture != Gesture::Tap;
    }
    if (touch.gesture == Gesture::SwipeDown && page_ != Page::Provisioning) {
        SetPage(page_ == Page::ConfirmWifi ? Page::Settings : Page::Face);
        return true;
    }
    if (touch.gesture != Gesture::Tap)
        return true;
    const auto hit = [&](int x, int y, int w, int h) {
        return touch.x >= x && touch.x < x + w && touch.y >= y && touch.y < y + h;
    };
    auto& board = Board::GetInstance();
    if (page_ == Page::Settings) {
        if (hit(48, 88, 50, 42) || hit(262, 88, 50, 42)) {
            auto codec = board.GetAudioCodec();
            codec->SetOutputVolume(
                std::clamp(codec->output_volume() + (touch.x < 180 ? -5 : 5), 0, 100));
        } else if (hit(48, 144, 50, 42) || hit(262, 144, 50, 42)) {
            if (auto light = board.GetBacklight())
                light->SetBrightness(
                    std::clamp(int(light->brightness()) + (touch.x < 180 ? -5 : 5), 10, 100), true);
        } else if (hit(85, 253, 190, 36)) {
            SetPage(Page::ConfirmWifi);
        } else if (hit(120, 302, 120, 32)) {
            SetPage(Page::Face);
        }
    } else if (page_ == Page::ConfirmWifi) {
        if (hit(90, 205, 180, 40) && enter_wifi_) {
            if (enter_wifi_())
                ShowProvisioning();
            else
                SetPage(Page::Settings);
        } else if (hit(110, 266, 140, 38)) {
            SetPage(Page::Settings);
        }
    } else if (hit(110, 309, 140, 32) && !leaving_config_ &&
               WifiManager::GetInstance().IsConfigMode() && leave_wifi_) {
        leaving_config_ = true;
        leave_wifi_();
    }
    Render();
    return true;  // 菜单中的每次触摸都被消费，不穿透到对话开关。
}
#endif

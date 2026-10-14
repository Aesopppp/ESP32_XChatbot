/*
 * 主题模型与主题注册表：集中保存颜色、字体、表情集合和背景图。
 * 字体与图片使用共享所有权；主题表保存主题指针，调用方需要维护主题对象的寿命。
 */

#pragma once

#include "display.h"
#include "emoji_collection.h"
#include "lvgl_font.h"
#include "lvgl_image.h"

#include <lvgl.h>
#include <map>
#include <memory>
#include <string>

class LvglTheme : public Theme {
public:
    // 把资源中的颜色字符串转成 LVGL 颜色；支持格式由实现限定。
    static lv_color_t ParseColor(const std::string& color);

    LvglTheme(const std::string& name);

    // Properties
    // 以下颜色分别控制背景、文字、各角色气泡、边框和低电量提示。
    inline lv_color_t background_color() const { return background_color_; }
    inline lv_color_t text_color() const { return text_color_; }
    inline lv_color_t chat_background_color() const { return chat_background_color_; }
    inline lv_color_t user_bubble_color() const { return user_bubble_color_; }
    inline lv_color_t assistant_bubble_color() const { return assistant_bubble_color_; }
    inline lv_color_t system_bubble_color() const { return system_bubble_color_; }
    inline lv_color_t system_text_color() const { return system_text_color_; }
    inline lv_color_t border_color() const { return border_color_; }
    inline lv_color_t low_battery_color() const { return low_battery_color_; }
    // 资源使用共享所有权；取出的图片 / 字体还依赖底层资源数据的有效寿命。
    inline std::shared_ptr<LvglImage> background_image() const { return background_image_; }
    inline std::shared_ptr<EmojiCollection> emoji_collection() const { return emoji_collection_; }
    inline std::shared_ptr<LvglFont> text_font() const { return text_font_; }
    virtual std::shared_ptr<LvglFont> GetTextFont() const override { return text_font_; }
    inline std::shared_ptr<LvglFont> icon_font() const { return icon_font_; }
    inline std::shared_ptr<LvglFont> large_icon_font() const { return large_icon_font_; }
    inline std::shared_ptr<LvglFont> emoji_font() const { return emoji_font_; }
    // 返回主题基础间距乘以布局尺度；单位为界面逻辑像素。
    inline int spacing(int scale) const { return spacing_ * scale; }

    inline void set_background_color(lv_color_t background) { background_color_ = background; }
    inline void set_text_color(lv_color_t text) { text_color_ = text; }
    inline void set_chat_background_color(lv_color_t chat_background) {
        chat_background_color_ = chat_background;
    }
    inline void set_user_bubble_color(lv_color_t user_bubble) { user_bubble_color_ = user_bubble; }
    inline void set_assistant_bubble_color(lv_color_t assistant_bubble) {
        assistant_bubble_color_ = assistant_bubble;
    }
    inline void set_system_bubble_color(lv_color_t system_bubble) {
        system_bubble_color_ = system_bubble;
    }
    inline void set_system_text_color(lv_color_t system_text) { system_text_color_ = system_text; }
    inline void set_border_color(lv_color_t border) { border_color_ = border; }
    inline void set_low_battery_color(lv_color_t low_battery) { low_battery_color_ = low_battery; }
    inline void set_background_image(std::shared_ptr<LvglImage> background_image) {
        background_image_ = background_image;
    }
    inline void set_emoji_collection(std::shared_ptr<EmojiCollection> emoji_collection) {
        emoji_collection_ = emoji_collection;
    }
    inline void set_text_font(std::shared_ptr<LvglFont> text_font) { text_font_ = text_font; }
    inline void set_icon_font(std::shared_ptr<LvglFont> icon_font) { icon_font_ = icon_font; }
    inline void set_large_icon_font(std::shared_ptr<LvglFont> large_icon_font) {
        large_icon_font_ = large_icon_font;
    }
    inline void set_emoji_font(std::shared_ptr<LvglFont> emoji_font) { emoji_font_ = emoji_font; }

private:
    int spacing_ = 2;

    // Colors
    lv_color_t background_color_;
    lv_color_t text_color_;
    lv_color_t chat_background_color_;
    lv_color_t user_bubble_color_;
    lv_color_t assistant_bubble_color_;
    lv_color_t system_bubble_color_;
    lv_color_t system_text_color_;
    lv_color_t border_color_;
    lv_color_t low_battery_color_;

    // Background image
    std::shared_ptr<LvglImage> background_image_ = nullptr;

    // fonts
    std::shared_ptr<LvglFont> text_font_ = nullptr;
    std::shared_ptr<LvglFont> icon_font_ = nullptr;
    std::shared_ptr<LvglFont> large_icon_font_ = nullptr;
    std::shared_ptr<LvglFont> emoji_font_ = nullptr;

    // Emoji collection
    std::shared_ptr<EmojiCollection> emoji_collection_ = nullptr;
};

class LvglThemeManager {
public:
    static LvglThemeManager& GetInstance() {
        static LvglThemeManager instance;
        return instance;
    }

    // 登记主题指针而非复制对象，同名注册替换映射；调用方负责主题对象寿命。
    void RegisterTheme(const std::string& theme_name, LvglTheme* theme);
    LvglTheme* GetTheme(const std::string& theme_name);

private:
    LvglThemeManager();
    // 建立默认明暗主题和字体，用于资源主题尚未加载的启动阶段。
    void InitializeDefaultThemes();

    std::map<std::string, LvglTheme*> themes_;
};

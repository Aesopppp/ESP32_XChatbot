/*
 * LVGL 字体包装：统一内置字体和资源分区中的二进制字体。
 * 内置字体只复制描述结构，底层字模仍由原资源持有；CBin 字体负责销毁其解码描述。
 * fallback 是借用指针，备用字体必须比引用它的界面和字体活得更久。
 */

#pragma once

#include <lvgl.h>

class LvglFont {
public:
    virtual const lv_font_t* font() const = 0;
    virtual void SetFallback(const lv_font_t* fallback) = 0;
    virtual ~LvglFont() = default;
};

// Built-in font
// 复制 lv_font_t 以便单独设置 fallback；原始字体位图仍是外部静态资源。
class LvglBuiltInFont : public LvglFont {
public:
    LvglBuiltInFont(const lv_font_t* font) : font_(*font) {}
    virtual const lv_font_t* font() const override { return &font_; }
    virtual void SetFallback(const lv_font_t* fallback) override { font_.fallback = fallback; }

private:
    lv_font_t font_{};
};

// 管理二进制字体描述的创建和销毁，原始资源映射仍需保持有效。
class LvglCBinFont : public LvglFont {
public:
    LvglCBinFont(void* data);
    virtual ~LvglCBinFont();
    virtual const lv_font_t* font() const override { return font_; }
    uint8_t bpp() const {
        if (font_ == nullptr || font_->dsc == nullptr) {
            return 0;
        }
        return static_cast<const lv_font_fmt_txt_dsc_t*>(font_->dsc)->bpp;
    }
    virtual void SetFallback(const lv_font_t* fallback) override {
        if (font_ != nullptr) {
            font_->fallback = fallback;
        }
    }

private:
    lv_font_t* font_;
};

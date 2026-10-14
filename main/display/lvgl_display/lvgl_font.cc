/*
 * LVGL 字体包装：统一内置字体和资源分区中的二进制字体。
 * 内置字体只复制描述结构，底层字模仍由原资源持有；CBin 字体负责销毁其解码描述。
 * fallback 是借用指针，备用字体必须比引用它的界面和字体活得更久。
 */

#include "lvgl_font.h"
#include <cbin_font.h>


LvglCBinFont::LvglCBinFont(void* data) {
    font_ = cbin_font_create(static_cast<uint8_t*>(data));
}

LvglCBinFont::~LvglCBinFont() {
    if (font_ != nullptr) {
        cbin_font_delete(font_);
    }
}
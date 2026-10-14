/*
 * 动态字形缓存：把远端下发的 Unicode 位图组织为 LVGL 备用字体。
 * 最多保留 256 个字形和 64 KiB 位图，超限时淘汰最早更新的条目。
 * 有 PSRAM 时允许跨批次保留；无 PSRAM 时减少长期占用，避免挤占网络和音频的内部 SRAM。
 */

#ifndef DYNAMIC_GLYPH_CACHE_H
#define DYNAMIC_GLYPH_CACHE_H

#include "display.h"

#include <lvgl.h>
#include <cstddef>
#include <cstdint>
#include <vector>

class DynamicGlyphCache {
public:
    // 双重上限避免服务端字形下发导致缓存无界增长，修改时需同步评估内部 SRAM 占用。
    static constexpr size_t kMaxGlyphs = 256;
    static constexpr size_t kMaxBitmapBytes = 64 * 1024;

    DynamicGlyphCache();
    lv_font_t* EnsureFont(const lv_font_t* base_font, uint8_t bpp);
    bool AddGlyphs(const std::vector<TextGlyph>& glyphs);
    void Clear();

private:
    struct Entry {
        uint32_t codepoint = 0;
        uint32_t adv_w = 0;
        uint16_t box_w = 0;
        uint16_t box_h = 0;
        int16_t ofs_x = 0;
        int16_t ofs_y = 0;
        TextGlyphVector<uint8_t> bitmap;
        // 记录最近一次插入 / 更新批次，用于超限淘汰；不是渲染回调访问计数。
        uint64_t last_use = 0;
    };

    void Rebuild();
    size_t BitmapBytes() const;

    bool initialized_ = false;
    bool retain_between_batches_ = false;
    uint8_t bpp_ = 0;
    uint64_t use_counter_ = 0;
    lv_font_t font_{};
    lv_font_fmt_txt_dsc_t dsc_{};
    TextGlyphVector<Entry> entries_;
    // 以下连续数组供 LVGL 借用，Rebuild 后统一更新描述结构中的指针。
    TextGlyphVector<uint8_t> bitmap_blob_;
    TextGlyphVector<uint16_t> unicode_list_;
    TextGlyphVector<lv_font_fmt_txt_glyph_dsc_t> glyph_dsc_;
    TextGlyphVector<lv_font_fmt_txt_cmap_t> cmaps_;
};

#endif

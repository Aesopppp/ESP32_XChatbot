/*
 * 远端动态字形的校验入口：检查协议版本、字体能力、尺寸与 Base64 位图。
 * JSON 只在解析期间借用，成功后以自有 bitmap 数据交给显示缓存；不把网络裸指针保存到 LVGL。
 */

#pragma once

#include "display/text_glyph.h"

#include <cJSON.h>

#include <cstdint>
#include <vector>

namespace TextGlyphPayload {

// 读取借用的 JSON，成功输出独立字形数据及位深；缺少字形对象时保持输出参数原值。
bool Parse(const cJSON* root, std::vector<TextGlyph>& result, uint8_t& bpp);

}  // namespace TextGlyphPayload

/*
 * 远端动态字形的校验入口：检查协议版本、字体能力、尺寸与 Base64 位图。
 * JSON 只在解析期间借用，成功后以自有 bitmap 数据交给显示缓存；不把网络裸指针保存到 LVGL。
 */

#include "text_glyph_payload.h"
#include "assets.h"

#include <esp_log.h>
#include <mbedtls/base64.h>

#include <cstring>
#include <utility>

#define TAG "TextGlyphPayload"

namespace TextGlyphPayload {

// 缺少 glyph_push 对象表示本消息没有字形更新，返回 true；不兼容或格式错误返回 false。
bool Parse(const cJSON* root, std::vector<TextGlyph>& result, uint8_t& bpp) {
    auto payload = cJSON_GetObjectItem(root, "glyph_push");
    if (!cJSON_IsObject(payload)) {
        return true;
    }
    auto version = cJSON_GetObjectItem(payload, "v");
    auto bundle = cJSON_GetObjectItem(payload, "bundle");
    auto size = cJSON_GetObjectItem(payload, "size");
    auto wire_bpp = cJSON_GetObjectItem(payload, "bpp");
    auto glyphs = cJSON_GetObjectItem(payload, "glyphs");
    auto capability = Assets::GetInstance().text_font_capability();
    if (!capability.glyph_push || !cJSON_IsNumber(version) || version->valueint != 1 ||
        !cJSON_IsString(bundle) || capability.bundle != bundle->valuestring ||
        !cJSON_IsNumber(size) || size->valuedouble != capability.size ||
        !cJSON_IsNumber(wire_bpp) || wire_bpp->valuedouble != capability.bpp ||
        !cJSON_IsArray(glyphs) || cJSON_GetArraySize(glyphs) > 64) {
        ESP_LOGW(TAG, "Rejected incompatible glyph payload");
        return false;
    }

    // 先构建临时结果，全部校验通过后才提交，失败不把半批字形写入显示缓存。
    const uint8_t parsed_bpp = static_cast<uint8_t>(wire_bpp->valueint);
    std::vector<TextGlyph> parsed;
    parsed.reserve(cJSON_GetArraySize(glyphs));
    size_t total_bitmap_bytes = 0;
    cJSON* item = nullptr;
    cJSON_ArrayForEach (item, glyphs) {
        auto codepoint = cJSON_GetObjectItem(item, "codepoint");
        auto adv_w = cJSON_GetObjectItem(item, "adv_w");
        auto box_w = cJSON_GetObjectItem(item, "box_w");
        auto box_h = cJSON_GetObjectItem(item, "box_h");
        auto ofs_x = cJSON_GetObjectItem(item, "ofs_x");
        auto ofs_y = cJSON_GetObjectItem(item, "ofs_y");
        auto bitmap = cJSON_GetObjectItem(item, "bitmap");
        if (!cJSON_IsNumber(codepoint) || codepoint->valuedouble <= 0 ||
            codepoint->valuedouble > 0x10FFFF || !cJSON_IsNumber(adv_w) || adv_w->valuedouble < 0 ||
            adv_w->valuedouble > UINT32_MAX || !cJSON_IsNumber(box_w) || box_w->valueint < 0 ||
            box_w->valueint > 64 || !cJSON_IsNumber(box_h) || box_h->valueint < 0 ||
            box_h->valueint > 64 || !cJSON_IsNumber(ofs_x) || ofs_x->valueint < INT16_MIN ||
            ofs_x->valueint > INT16_MAX || !cJSON_IsNumber(ofs_y) || ofs_y->valueint < INT16_MIN ||
            ofs_y->valueint > INT16_MAX || !cJSON_IsString(bitmap)) {
            ESP_LOGW(TAG, "Rejected malformed glyph");
            return false;
        }

        // 按位深计算最小位图字节数，并限制本批总量不超过 64 KiB。
        const size_t expected =
            (static_cast<size_t>(box_w->valueint) * box_h->valueint * parsed_bpp + 7) / 8;
        total_bitmap_bytes += expected;
        if (total_bitmap_bytes > 64 * 1024) {
            ESP_LOGW(TAG, "Rejected oversized glyph payload");
            return false;
        }

        TextGlyph glyph;
        glyph.codepoint = static_cast<uint32_t>(codepoint->valuedouble);
        glyph.adv_w = static_cast<uint32_t>(adv_w->valuedouble);
        glyph.box_w = static_cast<uint16_t>(box_w->valueint);
        glyph.box_h = static_cast<uint16_t>(box_h->valueint);
        glyph.ofs_x = static_cast<int16_t>(ofs_x->valueint);
        glyph.ofs_y = static_cast<int16_t>(ofs_y->valueint);
        glyph.bitmap.resize(expected);
        const size_t encoded_length = strlen(bitmap->valuestring);
        if (encoded_length > ((expected + 2) / 3) * 4 + 4) {
            ESP_LOGW(TAG, "Rejected oversized base64 bitmap");
            return false;
        }
        if (expected == 0) {
            if (encoded_length != 0) {
                return false;
            }
            parsed.push_back(std::move(glyph));
            continue;
        }
        size_t decoded = 0;
        // 解码实际长度必须与字形尺寸计算值完全一致，避免缓存读越界或错误字体布局。
        int rc = mbedtls_base64_decode(glyph.bitmap.data(), glyph.bitmap.size(), &decoded,
                                       reinterpret_cast<const unsigned char*>(bitmap->valuestring),
                                       encoded_length);
        if (rc != 0 || decoded != expected) {
            ESP_LOGW(TAG, "Rejected invalid bitmap for U+%04lX",
                     static_cast<unsigned long>(glyph.codepoint));
            return false;
        }
        parsed.push_back(std::move(glyph));
    }
    result = std::move(parsed);
    bpp = parsed_bpp;
    return true;
}

}  // namespace TextGlyphPayload

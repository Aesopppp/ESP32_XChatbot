/*
 * 动态字形的数据结构与专用分配器。
 * 字形包含 Unicode、字距、位图尺寸、偏移和像素；优先使用可用的 PSRAM 存储这些数据。
 * 分配器遇到长度溢出或分配失败会终止，网络入口必须先限制字形数量和位图大小。
 */

#pragma once

#include <esp_heap_caps.h>
#include <esp_system.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

bool TextGlyphStorageUsesPsram();

template <typename T>
class TextGlyphAllocator {
public:
    using value_type = T;
    using is_always_equal = std::true_type;

    TextGlyphAllocator() noexcept = default;

    template <typename U>
    TextGlyphAllocator(const TextGlyphAllocator<U>&) noexcept {}

    // 先检查乘法溢出，再依据实际 PSRAM 可用性选择 heap_caps 分配能力。
    T* allocate(size_t count) {
        if (count == 0) {
            return nullptr;
        }
        if (count > std::numeric_limits<size_t>::max() / sizeof(T)) {
            esp_system_abort("TextGlyphAllocator allocation size overflow");
        }
        uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
        if (TextGlyphStorageUsesPsram()) {
            caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        }
        auto ptr = static_cast<T*>(heap_caps_malloc(count * sizeof(T), caps));
        if (ptr == nullptr) {
            esp_system_abort("TextGlyphAllocator failed to allocate glyph storage");
        }
        return ptr;
    }

    void deallocate(T* ptr, size_t) noexcept { heap_caps_free(ptr); }
};

template <typename T, typename U>
bool operator==(const TextGlyphAllocator<T>&, const TextGlyphAllocator<U>&) {
    return true;
}

template <typename T, typename U>
bool operator!=(const TextGlyphAllocator<T>&, const TextGlyphAllocator<U>&) {
    return false;
}

template <typename T>
using TextGlyphVector = std::vector<T, TextGlyphAllocator<T>>;

// 一份字形的布局指标与像素数据；adv_w 保留协议传入的 LVGL 字距单位。
struct TextGlyph {
    uint32_t codepoint = 0;
    uint32_t adv_w = 0;
    uint16_t box_w = 0;
    uint16_t box_h = 0;
    int16_t ofs_x = 0;
    int16_t ofs_y = 0;
    TextGlyphVector<uint8_t> bitmap;
};

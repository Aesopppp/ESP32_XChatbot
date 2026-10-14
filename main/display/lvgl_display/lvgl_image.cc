/*
 * LVGL 图像包装：明确图像描述符和像素缓冲区的所有权。
 * RawImage 借用原数据；CBinImage 管理解码描述；AllocatedImage 接管动态分配的缓冲区并释放。
 * 将图像交给控件后需保留包装对象，避免 LVGL 后续刷新访问已经释放的像素。
 */

#include "lvgl_image.h"
#include <cbin_font.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cstring>

#define TAG "LvglImage"

// 只建立 RAW_ALPHA 描述符，不复制像素；解码时再由 LVGL 获取尺寸，原数据必须保持有效。
LvglRawImage::LvglRawImage(void* data, size_t size) {
    bzero(&image_dsc_, sizeof(image_dsc_));
    image_dsc_.data_size = size;
    image_dsc_.data = static_cast<uint8_t*>(data);
    image_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    image_dsc_.header.cf = LV_COLOR_FORMAT_RAW_ALPHA;
    image_dsc_.header.w = 0;
    image_dsc_.header.h = 0;
}

// 这里只比较前三字节标记，调用前需保证数据非空且至少有三字节；不是完整 GIF 校验。
bool LvglRawImage::IsGif() const {
    auto ptr = (const uint8_t*)image_dsc_.data;
    return ptr[0] == 'G' && ptr[1] == 'I' && ptr[2] == 'F';
}

LvglCBinImage::LvglCBinImage(void* data) {
    image_dsc_ = cbin_img_dsc_create(static_cast<uint8_t*>(data));
}

// 销毁 cbin 创建的描述符；底层资源的寿命仍须遵守资源库的挂载规则。
LvglCBinImage::~LvglCBinImage() {
    if (image_dsc_ != nullptr) {
        cbin_img_dsc_delete(image_dsc_);
    }
}

// 接管缓冲后尝试识别图像格式；失败时 valid_ 保持 false，析构仍释放已经接管的数据。
LvglAllocatedImage::LvglAllocatedImage(void* data, size_t size) {
    bzero(&image_dsc_, sizeof(image_dsc_));
    image_dsc_.data_size = size;
    image_dsc_.data = static_cast<uint8_t*>(data);

    if (lv_image_decoder_get_info(&image_dsc_, &image_dsc_.header) != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to get image info, data: %p size: %u", data, size);
        return;
    }
    valid_ = true;
}

// 已知像素格式时直接填写描述符；stride 按字节计，包含一行末尾的对齐填充。
LvglAllocatedImage::LvglAllocatedImage(void* data, size_t size, int width, int height, int stride,
                                       int color_format) {
    bzero(&image_dsc_, sizeof(image_dsc_));
    image_dsc_.data_size = size;
    image_dsc_.data = static_cast<uint8_t*>(data);
    image_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    image_dsc_.header.cf = color_format;
    image_dsc_.header.w = width;
    image_dsc_.header.h = height;
    image_dsc_.header.stride = stride;
    valid_ = data != nullptr && size > 0 && width > 0 && height > 0;
}

// 释放接管的像素数据；仍引用此描述符的控件须先切换资源，避免后续刷新访问悬空指针。
LvglAllocatedImage::~LvglAllocatedImage() {
    if (image_dsc_.data) {
        heap_caps_free((void*)image_dsc_.data);
        image_dsc_.data = nullptr;
    }
}

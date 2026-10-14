/*
 * LVGL 图像包装：明确图像描述符和像素缓冲区的所有权。
 * RawImage 借用原数据；CBinImage 管理解码描述；AllocatedImage 接管动态分配的缓冲区并释放。
 * 将图像交给控件后需保留包装对象，避免 LVGL 后续刷新访问已经释放的像素。
 */

#pragma once

#include <lvgl.h>

// Wrap around lv_img_dsc_t
class LvglImage {
public:
    virtual const lv_img_dsc_t* image_dsc() const = 0;
    virtual bool IsGif() const { return false; }
    virtual ~LvglImage() = default;
};

// 只包装外部提供的原始数据，不释放像素；常用于资源映射地址或 GIF 压缩数据。
class LvglRawImage : public LvglImage {
public:
    LvglRawImage(void* data, size_t size);
    virtual const lv_img_dsc_t* image_dsc() const override { return &image_dsc_; }
    virtual bool IsGif() const;

private:
    lv_img_dsc_t image_dsc_;
};

// 从 CBin 数据建立并拥有图像描述，析构时调用对应解码器释放接口。
class LvglCBinImage : public LvglImage {
public:
    LvglCBinImage(void* data);
    virtual ~LvglCBinImage();
    virtual const lv_img_dsc_t* image_dsc() const override { return image_dsc_; }

private:
    lv_img_dsc_t* image_dsc_ = nullptr;
};

// 接管 heap_caps 兼容的动态缓冲；即使描述校验不通过也由此对象负责释放。
class LvglAllocatedImage : public LvglImage {
public:
    LvglAllocatedImage(void* data, size_t size);
    LvglAllocatedImage(void* data, size_t size, int width, int height, int stride,
                       int color_format);
    virtual ~LvglAllocatedImage();
    virtual const lv_img_dsc_t* image_dsc() const override { return &image_dsc_; }
    bool IsValid() const { return valid_; }

private:
    lv_img_dsc_t image_dsc_;
    bool valid_ = false;
};

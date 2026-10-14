/*
 * GIF 与 LVGL 的连接层：管理解码画布、帧定时器和循环播放。
 * gifdec.c / gifdec.h / gifdec_mve.h 是底层第三方解码器，保留原实现；本包装负责生命周期。
 * LVGL 图像描述直接指向解码画布，关闭解码器前必须停止定时器并解除界面引用。
 */

#pragma once

#include "../lvgl_image.h"
#include "gifdec.h"
#include <lvgl.h>
#include <memory>
#include <functional>

/**
 * C++ implementation of LVGL GIF widget
 * Provides GIF animation functionality using gifdec library
 */
class LvglGif {
public:
    // 从传入图片数据建立解码器；原始 GIF 数据必须在解码器使用期间持续有效。
    explicit LvglGif(const lv_img_dsc_t* img_dsc);
    virtual ~LvglGif();

    // LvglImage interface implementation
    // 返回内部描述符的借用指针；加载失败时为 nullptr，不能由调用方释放。
    virtual const lv_img_dsc_t* image_dsc() const;

    /**
     * Start/restart GIF animation
     */
    // 创建或恢复 LVGL 帧定时器并尝试推进当前帧；调用应与 LVGL 访问保持同步。
    void Start();

    /**
     * Pause GIF animation
     */
    // 暂停定时器并保留当前解码位置，不释放画布和 GIF 数据。
    void Pause();

    /**
     * Resume GIF animation
     */
    // 恢复已存在的定时器；尚未 Start 创建定时器时不会自动创建。
    void Resume();

    /**
     * Stop GIF animation and rewind to first frame
     */
    // 暂停动画并回退 GIF 读取位置，保留解码资源以便后续继续使用。
    void Stop();

    /**
     * Check if GIF is currently playing
     */
    // 只反映本包装的播放标志，循环等待期间也可能为 true。
    bool IsPlaying() const;

    /**
     * Check if GIF was loaded successfully
     */
    // 判断解码器与描述符是否成功建立，不能用来判断动画正在运行。
    bool IsLoaded() const;

    /**
     * Get loop count
     */
    // 取得解码器当前循环计数，未加载时返回 -1；具体次数语义由 gifdec 管理。
    int32_t GetLoopCount() const;

    /**
     * Set loop count
     */
    // 直接修改解码器的循环计数，不会自动重建已经暂停的定时器。
    void SetLoopCount(int32_t count);

    /**
     * Get loop delay in milliseconds (delay between loops)
     */
    // 返回两轮动画之间设置的额外间隔，单位毫秒。
    uint32_t GetLoopDelay() const;

    /**
     * Set loop delay in milliseconds (delay between loops)
     * @param delay_ms Delay in milliseconds before starting next loop. 0 means no delay.
     */
    // 设置循环间隔，0 表示不额外等待；每帧自身延时仍来自 GIF 文件。
    void SetLoopDelay(uint32_t delay_ms);

    /**
     * Get GIF dimensions
     */
    // 返回 GIF 画布宽度，单位像素；未加载时为 0。
    uint16_t width() const;
    // 返回 GIF 画布高度，单位像素；未加载时为 0。
    uint16_t height() const;

    /**
     * Set frame update callback
     */
    // 帧渲染完成时在 LVGL 定时器上下文调用；回调应尽快请求刷新，避免阻塞音频或网络。
    void SetFrameCallback(std::function<void()> callback);

private:
    // GIF decoder instance
    // 本包装拥有解码器，Cleanup 先删除定时器再关闭解码器，防止回调访问已释放画布。
    gd_GIF* gif_;
    
    // LVGL image descriptor
    lv_img_dsc_t img_dsc_;
    
    // Animation timer
    lv_timer_t* timer_;
    
    // Last frame update time
    uint32_t last_call_;
    
    // Animation state
    bool playing_;
    bool loaded_;
    
    // Loop delay configuration
    uint32_t loop_delay_ms_;      // Delay between loops in milliseconds
    bool loop_waiting_;           // Whether we're waiting for the next loop
    uint32_t loop_wait_start_;    // Timestamp when loop wait started
    
    // Frame update callback
    // 回调若捕获外部控件或对象，外部对象必须比 GIF 包装活得更久或提前解除回调。
    std::function<void()> frame_callback_;
    
    /**
     * Update to next frame
     */
    void NextFrame();
    
    /**
     * Cleanup resources
     */
    void Cleanup();
};

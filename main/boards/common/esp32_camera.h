/*
 * esp_camera 驱动的摄像头适配，实现抓帧、RGB565 转 JPEG 和图像解释请求。
 * 图像压缩可在独立线程中完成；帧缓冲来自摄像头驱动，需按驱动要求归还，不能直接 free。
 * 当前 VoCat 语音功能不要求安装摄像头，只有板级创建此对象时才启用。
 */

#pragma once
#include "sdkconfig.h"

#include <lvgl.h>
#include <memory>
#include <thread>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "camera.h"
#include "esp_camera.h"
#include "jpg/image_to_jpeg.h"

struct JpegChunk {
    uint8_t* data;
    size_t len;
};

class Esp32Camera : public Camera {
private:
    bool streaming_on_ = false;
    bool swap_bytes_enabled_ = true;  // Swap pixel byte order for RGB565, enabled by default
    std::string explain_url_;
    std::string explain_token_;
    // Capture 会等待上一轮编码；调用方须确保对象销毁前编码已完成，避免后台任务访问已释放帧。
    std::thread encoder_thread_;
    // 由 esp_camera 借出的帧，通过 esp_camera_fb_return 归还而非 heap_caps_free。
    camera_fb_t* current_fb_ = nullptr;
    uint8_t* encode_buf_ = nullptr;  // Buffer for JPEG encoding (with optional byte swap)
    size_t encode_buf_size_ = 0;

public:
    Esp32Camera(const camera_config_t& config);
    ~Esp32Camera();

    virtual void SetExplainUrl(const std::string& url, const std::string& token) override;
    virtual bool Capture() override;
    virtual bool SetHMirror(bool enabled) override;
    virtual bool SetVFlip(bool enabled) override;
    virtual bool SetSwapBytes(bool enabled) override;
    virtual std::expected<std::string, std::string> Explain(const std::string& question) override;
};

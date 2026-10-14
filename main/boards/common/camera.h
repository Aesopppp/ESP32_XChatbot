/*
 * 可选摄像头抽象：拍摄、方向设置和向图像解释服务提交问题。
 * 不是麦克风接口；板上无摄像头时 Board 返回空指针，应用必须检查该能力。
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <expected>
#include <string>

class Camera {
public:
    virtual ~Camera() = default;

    virtual void SetExplainUrl(const std::string& url, const std::string& token) = 0;
    virtual bool Capture() = 0;
    virtual bool SetHMirror(bool enabled) = 0;
    virtual bool SetVFlip(bool enabled) = 0;
    virtual bool SetSwapBytes(bool enabled) { return false; }  // Optional, default no-op
    virtual std::expected<std::string, std::string> Explain(const std::string& question) = 0;
};

#endif  // CAMERA_H

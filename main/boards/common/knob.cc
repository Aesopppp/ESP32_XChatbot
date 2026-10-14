/*
 * 旋转编码器包装：读取 A / B 相变化并回调旋转方向。
 * OnRotate 的布尔参数为 true 表示 KNOB_RIGHT；旋转对应音量还是其他功能由调用方选择。
 */

#include "knob.h"

static const char* TAG = "Knob";

Knob::Knob(gpio_num_t pin_a, gpio_num_t pin_b) {
    knob_config_t config = {
        .default_direction = 0,
        .gpio_encoder_a = static_cast<uint8_t>(pin_a),
        .gpio_encoder_b = static_cast<uint8_t>(pin_b),
    };

    esp_err_t err = ESP_OK;
    knob_handle_ = iot_knob_create(&config);
    if (knob_handle_ == NULL) {
        ESP_LOGE(TAG, "Failed to create knob instance");
        return;
    }

    err = iot_knob_register_cb(knob_handle_, KNOB_LEFT, knob_callback, this);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register left callback: %s", esp_err_to_name(err));
        return;
    }

    err = iot_knob_register_cb(knob_handle_, KNOB_RIGHT, knob_callback, this);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register right callback: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Knob initialized with pins A:%d B:%d", pin_a, pin_b);
}

Knob::~Knob() {
    if (knob_handle_ != NULL) {
        iot_knob_delete(knob_handle_);
        knob_handle_ = NULL;
    }
}

void Knob::OnRotate(std::function<void(bool)> callback) {
    on_rotate_ = callback;
}

// 从组件事件读取方向，再调用板级业务回调；不在本层写死音量步长。
void Knob::knob_callback(void* arg, void* data) {
    Knob* knob = static_cast<Knob*>(data);
    knob_event_t event = iot_knob_get_event(arg);
    
    if (knob->on_rotate_) {
        knob->on_rotate_(event == KNOB_RIGHT);
    }
}
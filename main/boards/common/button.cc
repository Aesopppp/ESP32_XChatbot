/*
 * 物理按键包装：把 iot_button 的 GPIO / ADC 事件转换为 C++ 回调。
 * 模块处理按下、松开、单击、长按和多击；业务动作由板级注册的回调决定。
 * 回调由按键组件调用，不能假定处于应用主任务；修改对话状态时应通过 Application 调度。
 */

#include "button.h"

#include <button_gpio.h>
#include <esp_log.h>

#define TAG "Button"

#if CONFIG_SOC_ADC_SUPPORTED
AdcButton::AdcButton(const button_adc_config_t& adc_config) : Button(nullptr) {
    button_config_t btn_config = {
        .long_press_time = 2000,
        .short_press_time = 0,
    };
    ESP_ERROR_CHECK(iot_button_new_adc_device(&btn_config, &adc_config, &button_handle_));
}
#endif

Button::Button(button_handle_t button_handle) : button_handle_(button_handle) {
}

Button::Button(gpio_num_t gpio_num, bool active_high, uint16_t long_press_time, uint16_t short_press_time, bool enable_power_save) : gpio_num_(gpio_num) {
    if (gpio_num == GPIO_NUM_NC) {
        return;
    }
    button_config_t button_config = {
        .long_press_time = long_press_time,
        .short_press_time = short_press_time
    };
    button_gpio_config_t gpio_config = {
        .gpio_num = gpio_num,
        .active_level = static_cast<uint8_t>(active_high ? 1 : 0),
        .enable_power_save = enable_power_save,
        .disable_pull = false
    };
    ESP_ERROR_CHECK(iot_button_new_gpio_device(&button_config, &gpio_config, &button_handle_));
}

// 包装对象拥有 button_handle，即使句柄由外部构造传入，析构也会删除底层按键。
Button::~Button() {
    if (button_handle_ != NULL) {
        iot_button_delete(button_handle_);
    }
}

// 注册电平按下事件，适合按住说话的开始动作；事件消抖由按键组件处理。
void Button::OnPressDown(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_press_down_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_PRESS_DOWN, nullptr, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_press_down_) {
            button->on_press_down_();
        }
    }, this);
}

// 注册松开事件，适合与按下配对实现结束动作。
void Button::OnPressUp(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_press_up_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_PRESS_UP, nullptr, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_press_up_) {
            button->on_press_up_();
        }
    }, this);
}

// 注册长按达到阈值的开始事件；这里不是每次按住轮询都反复执行。
void Button::OnLongPress(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_long_press_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_LONG_PRESS_START, nullptr, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_long_press_) {
            button->on_long_press_();
        }
    }, this);
}

// 注册完成的单击手势，区别于刚按下时的 OnPressDown。
void Button::OnClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_click_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_SINGLE_CLICK, nullptr, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_click_) {
            button->on_click_();
        }
    }, this);
}

// 注册双击手势，具体判定时间由按键组件负责。
void Button::OnDoubleClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_double_click_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_DOUBLE_CLICK, nullptr, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_double_click_) {
            button->on_double_click_();
        }
    }, this);
}

// 向组件传入指定连续点击次数，同一包装对象保存此多击回调。
void Button::OnMultipleClick(std::function<void()> callback, uint8_t click_count) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_multiple_click_ = callback;
    button_event_args_t event_args = {
        .multiple_clicks = {
            .clicks = click_count
        }
    };
    iot_button_register_cb(button_handle_, BUTTON_MULTIPLE_CLICK, &event_args, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_multiple_click_) {
            button->on_multiple_click_();
        }
    }, this);
}
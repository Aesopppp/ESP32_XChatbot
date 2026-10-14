/*
 * 主题模型与主题注册表：集中保存颜色、字体、表情集合和背景图。
 * 字体与图片使用共享所有权；主题表保存主题指针，调用方需要维护主题对象的寿命。
 */

#include "lvgl_theme.h"

LvglTheme::LvglTheme(const std::string& name) : Theme(name) {
}

// 把 # 开头的颜色字符串转换为 RGB；缺少前缀或长度不足时返回黑色。
lv_color_t LvglTheme::ParseColor(const std::string& color) {
    // A shorter value such as "#fff" would make substr() throw
    // std::out_of_range, which aborts because C++ exceptions are disabled.
    if (color.size() >= 7 && color[0] == '#') {
        // Convert #112233 to lv_color_t
        uint8_t r = strtol(color.substr(1, 2).c_str(), nullptr, 16);
        uint8_t g = strtol(color.substr(3, 2).c_str(), nullptr, 16);
        uint8_t b = strtol(color.substr(5, 2).c_str(), nullptr, 16);
        return lv_color_make(r, g, b);
    }
    return lv_color_black();
}

LvglThemeManager::LvglThemeManager() {
}

// 按名称查询已注册主题，未注册返回空指针；不会自动创建新主题。
LvglTheme* LvglThemeManager::GetTheme(const std::string& theme_name) {
    auto it = themes_.find(theme_name);
    if (it != themes_.end()) {
        return it->second;
    }
    return nullptr;
}

// 保存主题引用而非复制；注册对象应持续存活，重复名称会替换指针。
void LvglThemeManager::RegisterTheme(const std::string& theme_name, LvglTheme* theme) {
    themes_[theme_name] = theme;
}

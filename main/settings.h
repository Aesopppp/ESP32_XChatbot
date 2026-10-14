/*
 * NVS 设置封装：按命名空间读取持久化值，并在写对象析构时提交 Set 操作。
 * 默认只读；键名与存储类型属于已烧录设备的持久化接口，变更需要考虑迁移。
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include <nvs_flash.h>

class Settings {
public:
    // read_write 为 true 才允许写入；对象寿命内使用同一个 NVS 句柄。
    Settings(const std::string& ns, bool read_write = false);
    ~Settings();

    // 命名空间、键或类型不存在时返回 default_value；字符串读取不保留末尾零字符。
    std::string GetString(const std::string& key, const std::string& default_value = "");
    void SetString(const std::string& key, const std::string& value);
    int32_t GetInt(const std::string& key, int32_t default_value = 0);
    void SetInt(const std::string& key, int32_t value);
    bool GetBool(const std::string& key, bool default_value = false);
    void SetBool(const std::string& key, bool value);
    // 删除接口当前不会设置 dirty_；不能假定只调用删除就会触发析构提交。
    void EraseKey(const std::string& key);
    void EraseAll();

private:
    std::string ns_;
    nvs_handle_t nvs_handle_ = 0;
    bool read_write_ = false;
    // SetString / SetInt / SetBool 置位，析构时才执行 nvs_commit。
    bool dirty_ = false;
};

#endif

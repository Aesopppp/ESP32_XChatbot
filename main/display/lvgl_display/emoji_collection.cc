/*
 * 按表情名称查找图像的集合，供 LCD 的 SetEmotion 使用。
 * 集合在析构时删除保存的图像包装对象；传入对象不应再由其他代码重复删除。
 * 注册同名表情前应考虑旧对象的所有权，本实现的赋值不会自动删除被覆盖的旧指针。
 */

#include "emoji_collection.h"

#include <esp_log.h>

#define TAG "EmojiCollection"

void EmojiCollection::AddEmoji(const std::string& name, LvglImage* image) {
    emoji_collection_[name] = image;
}

const LvglImage* EmojiCollection::GetEmojiImage(const char* name) {
    auto it = emoji_collection_.find(name);
    if (it != emoji_collection_.end()) {
        return it->second;
    }

    ESP_LOGW(TAG, "Emoji not found: %s", name);
    return nullptr;
}

EmojiCollection::~EmojiCollection() {
    for (auto it = emoji_collection_.begin(); it != emoji_collection_.end(); ++it) {
        delete it->second;
    }
    emoji_collection_.clear();
}

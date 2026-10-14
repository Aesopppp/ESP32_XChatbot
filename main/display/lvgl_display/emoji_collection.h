/*
 * 按表情名称查找图像的集合，供 LCD 的 SetEmotion 使用。
 * 集合在析构时删除保存的图像包装对象；传入对象不应再由其他代码重复删除。
 * 注册同名表情前应考虑旧对象的所有权，本实现的赋值不会自动删除被覆盖的旧指针。
 */

#ifndef EMOJI_COLLECTION_H
#define EMOJI_COLLECTION_H

#include "lvgl_image.h"

#include <lvgl.h>

#include <map>
#include <memory>
#include <string>

// Define interface for emoji collection
class EmojiCollection {
public:
    // 按名称接管图片包装；同名覆盖前需处理旧对象，防止遗失释放入口。
    virtual void AddEmoji(const std::string& name, LvglImage* image);
    // 查询得到借用指针，未找到时返回 nullptr；引用期间应保持集合和图片有效。
    virtual const LvglImage* GetEmojiImage(const char* name);
    virtual ~EmojiCollection();

private:
    std::map<std::string, LvglImage*> emoji_collection_;
};

#endif

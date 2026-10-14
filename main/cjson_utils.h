/*
 * cJSON 的作用域所有权包装，确保 JSON 树和打印字符串采用对应的释放函数。
 * 根对象由 cJSON_Delete 释放，打印结果由 cJSON_free 释放；JSON 子节点通常借用父树。
 */

#ifndef CJSON_UTILS_H
#define CJSON_UTILS_H

#include <cJSON.h>

#include <memory>

struct CJsonDeleter {
    void operator()(cJSON* value) const {
        if (value != nullptr) {
            cJSON_Delete(value);
        }
    }
};

struct CJsonStringDeleter {
    void operator()(char* value) const {
        if (value != nullptr) {
            cJSON_free(value);
        }
    }
};

// 用于拥有整棵 JSON 树的根节点；GetObjectItem 返回的子节点不可直接交给它独立销毁。
using CJsonUniquePtr = std::unique_ptr<cJSON, CJsonDeleter>;
// 用于 cJSON_Print / PrintUnformatted 的返回字符串，不能使用 delete[] 代替 cJSON_free。
using CJsonStringUniquePtr = std::unique_ptr<char, CJsonStringDeleter>;

#endif  // CJSON_UTILS_H

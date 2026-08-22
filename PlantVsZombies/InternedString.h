#pragma once

#include <string>

/**
 * @brief 返回按内容去重、进程期地址稳定的字符串。
 * @details 供高数量对象保存冷名称指针；调用方不得尝试修改或释放返回值。
 */
const std::string& InternRuntimeString(const std::string& value);

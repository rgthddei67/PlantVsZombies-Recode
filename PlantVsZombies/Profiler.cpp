#include "Profiler.h"

// 性能埋点全局开关的唯一定义。
// 默认 false（Profiler 完全休眠）；main.cpp 解析到启动参数 -Profile 时置为 true。
bool g_ProfileEnabled = false;

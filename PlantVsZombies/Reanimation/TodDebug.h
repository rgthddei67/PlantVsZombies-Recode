#pragma once

#ifndef _TODDEBUG_H
#define _TODDEBUG_H
#include <cassert>
#include <cstdio>
#include "../Logger.h"

#define TOD_ASSERT(expr) assert(expr)

// 重定向到统一日志（Reanim 子系统，Debug 级）
#define TOD_TRACE(msg) LOG_DEBUG("Reanim") << msg

// 保留 printf 风格语义，但走 logger 缓冲：格式化到栈缓冲再交给 LOG_DEBUG
#define TOD_TRACE_FORMAT(fmt, ...)                                  \
    do {                                                            \
        char _todBuf[512];                                          \
        std::snprintf(_todBuf, sizeof(_todBuf), fmt, __VA_ARGS__);  \
        LOG_DEBUG("Reanim") << _todBuf;                             \
    } while (0)

inline void TodErrorMessageBox(const char* message, const char* title) {
    LOG_ERROR("Reanim") << title << ": " << message;
}

#endif

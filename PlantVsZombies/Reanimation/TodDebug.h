#pragma once

#ifndef _TODDEBUG_H
#define _TODDEBUG_H
#include "../AllCppInclude.h"
#include <iostream>
#include <cassert>

#define TOD_ASSERT(expr) assert(expr)
#define TOD_TRACE(msg) std::cout << msg << std::endl
#define TOD_TRACE_FORMAT(fmt, ...) printf(fmt, __VA_ARGS__)

inline void TodErrorMessageBox(const char* message, const char* title) {
    std::cerr << title << ": " << message << std::endl;
}

#endif
#include "Logger.h"
#include <iostream>
#include <mutex>
#include <atomic>

namespace {
    std::mutex g_logMutex;
    std::atomic<LogLevel> g_runtimeLevel{ LogLevel::Trace }; // 默认放行全部（编译期地板已先过滤）
}

bool Logger::ShouldLog(LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(g_runtimeLevel.load(std::memory_order_relaxed));
}

void Logger::SetLevel(LogLevel level) {
    g_runtimeLevel.store(level, std::memory_order_relaxed);
}

LogLevel Logger::GetLevel() {
    return g_runtimeLevel.load(std::memory_order_relaxed);
}

const char* Logger::LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

void Logger::Write(LogLevel level, const char* tag, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::cout << '[' << LevelName(level) << "][" << tag << "] " << msg << '\n';
    if (level >= LogLevel::Warn) {
        std::cout.flush(); // Warn/Error 立即落屏，保证崩溃前可见；高频 Trace/Debug 不 flush 省开销
    }
}

#pragma once
#include <sstream>
#include <string>

// 级别枚举：混合大小写，规避 wingdi.h 的 ERROR 宏冲突
enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4 };

// 编译期地板：Debug 全开；Release 只留 Warn/Error
#ifdef _DEBUG
  #define LOG_MIN_LEVEL ::LogLevel::Trace
#else
  #define LOG_MIN_LEVEL ::LogLevel::Warn
#endif

// 编译期常量：该级别是否参与编译
constexpr bool LogCompileEnabled(LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(LOG_MIN_LEVEL);
}

// 唯一共享状态封装（全静态，状态实体在 Logger.cpp 的文件作用域）
class Logger {
public:
    static bool ShouldLog(LogLevel level);                                   // 运行期软阈值
    static void SetLevel(LogLevel level);                                    // 运行期下限设置
    static LogLevel GetLevel();
    static const char* LevelName(LogLevel level);
    static void Write(LogLevel level, const char* tag, const std::string& msg);
};

// RAII 流：拼接到自身 buffer，析构时一次性加锁写出
class LogStream {
public:
    LogStream(LogLevel level, const char* tag) : mLevel(level), mTag(tag) {}
    ~LogStream() { Logger::Write(mLevel, mTag, mBuffer.str()); }

    template <typename T>
    LogStream& operator<<(const T& value) { mBuffer << value; return *this; }

    // 支持 std::hex / std::setw 等 ostream 操纵符
    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) { mBuffer << manip; return *this; }

private:
    LogLevel mLevel;
    const char* mTag;
    std::ostringstream mBuffer;
};

// 把 LogStream& 吃成 void，使三元两分支类型一致；operator& 优先级低于 <<
class LogVoidify {
public:
    void operator&(LogStream&) {}
};

#define LOG_AT(level, tag)                                                    \
    ( !::LogCompileEnabled(level) || !::Logger::ShouldLog(level) )            \
        ? (void)0                                                             \
        : ::LogVoidify() & ::LogStream((level), (tag))

#define LOG_TRACE(tag) LOG_AT(::LogLevel::Trace, tag)
#define LOG_DEBUG(tag) LOG_AT(::LogLevel::Debug, tag)
#define LOG_INFO(tag)  LOG_AT(::LogLevel::Info,  tag)
#define LOG_WARN(tag)  LOG_AT(::LogLevel::Warn,  tag)
#define LOG_ERROR(tag) LOG_AT(::LogLevel::Error, tag)

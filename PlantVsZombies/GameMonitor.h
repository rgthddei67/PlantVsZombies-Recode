#pragma once
#ifndef _GAME_MONITOR_H
#define _GAME_MONITOR_H
#include "CrashHandler.h"
#include <atomic>
#include <array>

struct StackUsageInfo {
    ULONG_PTR stackLow;
    ULONG_PTR stackHigh;
    SIZE_T totalReserved;
    SIZE_T used;
    SIZE_T remaining;
    double usagePercent;
};

struct MemoryStatus {
    SIZE_T totalPhysical;
    SIZE_T availablePhysical;
    SIZE_T totalPageFile;
    SIZE_T availablePageFile;
    SIZE_T totalVirtual;
    SIZE_T availableVirtual;
    SIZE_T processMemoryUsage;
    SIZE_T peakProcessMemory;
    DWORD  memoryLoad;
};

struct ProcessInfo {
    DWORD processId;
    DWORD threadCount;
    DWORD handleCount;
    SIZE_T workingSetSize;
    SIZE_T pagefileUsage;
    std::string executablePath;
};

// 帧时间滑动窗口大小
static constexpr int kFrameWindowSize = 60;

struct PerformanceMetrics {
    double   frameRate;
    double   frameTime;
    double   frameTimeMin;     // 新增：窗口内最小帧时间
    double   frameTimeMax;     // 新增：窗口内最大帧时间
    double   frameTimeStdDev;  // 新增：帧时间标准差（抖动指标）
    ULONGLONG totalFrames;
    ULONGLONG lastUpdateTime;
};

// 新增：可配置健康阈值
struct HealthThresholds {
    double   stackUsagePercent = 80.0;
    DWORD    memoryLoadPercent = 85;
    double   minFrameRate = 30.0;
    SIZE_T   maxPrivateBytes = 500ULL * 1024 * 1024; // 500 MB
    double   maxCpuUsagePercent = 80.0;
};

class GameMonitor {
private:
    static PerformanceMetrics    performance;
    static std::atomic<ULONGLONG> frameCount;
    static ULONGLONG             lastFrameTime;
    static HealthThresholds      thresholds;

    // 帧时间滑动窗口（线程安全由调用方保证——UpdateFrame 单线程调用）
    static std::array<double, kFrameWindowSize> frameTimeWindow;
    static int    frameWindowIndex;
    static bool   frameWindowFull;

    // 保护 performance 结构体的读写锁
    static SRWLOCK perfLock;

    // CPU 时间缓存（用于计算增量 CPU 占用）
    static ULONGLONG lastCpuKernel;
    static ULONGLONG lastCpuUser;
    static ULONGLONG lastCpuSystem;

public:
    // ---- 初始化 ----
    static void Init(); // 必须在使用前调用一次

    // ---- 健康阈值 ----
    static void SetHealthThresholds(const HealthThresholds& t);

    // ---- 栈监控 ----
    static void          PrintStackUsage();
    static StackUsageInfo GetStackUsage();

    // ---- 堆监控 ----
    static void   PrintHeapUsage();
    static SIZE_T GetProcessHeapUsage();
    static SIZE_T GetPrivateBytes();

    // ---- 系统内存 ----
    static void         PrintSystemMemoryStatus();
    static MemoryStatus GetSystemMemoryStatus();

    // ---- 进程信息 ----
    static void        PrintProcessInfo();
    static ProcessInfo GetProcessInfo();

    // ---- 性能（帧率 + 抖动） ----
    static void              UpdateFrame();
    static void              PrintPerformanceMetrics();
    static PerformanceMetrics GetPerformanceMetrics();

    // ---- CPU 占用（新增） ----
    static double GetCpuUsage();
    static void   PrintCpuUsage();

    // ---- 线程 ----
    static DWORD GetThreadCount();
    static void  PrintThreadInfo();

    // ---- 模块 ----
    static void PrintLoadedModules();

    // ---- 资源泄漏检测 ----
    static void CheckResourceLeaks();

    // ---- 健康检查 ----
    static void CheckSystemHealth();

    // ---- 综合报告 ----
    static void PrintComprehensiveReport();
};

#endif
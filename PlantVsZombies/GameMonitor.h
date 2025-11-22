#pragma once
#ifndef _GAME_MONITOR_H
#define _GAME_MONITOR_H
#include "CrashHandler.h"

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
    DWORD memoryLoad; // 内存使用百分比 0-100
};

struct ProcessInfo {
    DWORD processId;
    DWORD threadCount;
    DWORD handleCount;
    SIZE_T workingSetSize;
    SIZE_T pagefileUsage;
    std::string executablePath;
};

struct PerformanceMetrics {
    double frameRate;
    double frameTime;
    ULONGLONG totalFrames;
    ULONGLONG lastUpdateTime;
};

class GameMonitor {
private:
    static PerformanceMetrics performance;
    static ULONGLONG frameCount;
    static ULONGLONG lastFrameTime;

public:
	// 打印堆栈情况
	static void PrintStackUsage();
	// 获取堆栈情况
	static StackUsageInfo GetStackUsage();
    // 堆监控
    static void PrintHeapUsage();
    static SIZE_T GetProcessHeapUsage();
    static SIZE_T GetPrivateBytes();

    // 系统内存监控
    static void PrintSystemMemoryStatus();
    static MemoryStatus GetSystemMemoryStatus();

    // 进程信息
    static void PrintProcessInfo();
    static ProcessInfo GetProcessInfo();

    // 性能监控
    static void UpdateFrame();
    static void PrintPerformanceMetrics();
    static PerformanceMetrics GetPerformanceMetrics();

    // 完整系统报告
    static void PrintComprehensiveReport();

    // 线程监控
    static DWORD GetThreadCount();
    static void PrintThreadInfo();

    // 模块监控
    static void PrintLoadedModules();

    // 资源泄漏检测
    static void CheckResourceLeaks();

    // 监控警报
    static void CheckSystemHealth();
};

#endif
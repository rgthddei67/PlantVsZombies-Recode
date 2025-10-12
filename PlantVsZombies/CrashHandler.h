#pragma once
#ifndef _CRASH_HANDLER_H
#define _CRASH_HANDLER_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>

class CrashHandler {
private:
    static PVOID vehHandle;
    static LONG64 lastUACTime;

public:
    // 初始化崩溃处理器
    static void Initialize();

    // 清理崩溃处理器
    static void Cleanup();

private:
    // VEH 异常处理函数
    static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS exceptionInfo);

    // 处理崩溃的主函数
    static void HandleCrash(PEXCEPTION_POINTERS exceptionInfo);

    // 是不是崩溃异常
    static bool IsCrashException(DWORD exceptionCode);

    // 生成崩溃报告
    static std::string GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo);

    // 显示崩溃对话框
    static void ShowCrashDialog(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath);

    // 获取异常代码的描述
    static std::string GetExceptionCodeString(DWORD exceptionCode);

    // 获取栈跟踪信息
    static std::string GetStackTrace(PEXCEPTION_POINTERS exceptionInfo);

    // 获取模块名称
    static std::string GetModuleName(HMODULE module);

    static bool IsUACByTimeWindow(PEXCEPTION_POINTERS exceptionInfo);
};

#endif
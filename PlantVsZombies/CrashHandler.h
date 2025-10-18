#pragma once
#ifndef _CRASH_HANDLER_H
#define _CRASH_HANDLER_H
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <sstream>
#include <DbgHelp.h>
#include <shellapi.h>

class CrashHandler {
private:
    static LPTOP_LEVEL_EXCEPTION_FILTER previousFilter;

public:
    static void Initialize();
    static void Cleanup();

private:
    static LONG WINAPI UnhandledExceptionFilter(PEXCEPTION_POINTERS exceptionInfo);
    static std::string GetExceptionCodeString(DWORD exceptionCode);
    static std::string GetModuleName(HMODULE module);
    static void GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath);
    static void ShowCrashDialog(const std::string& errorMessage, const std::string& reportPath);
    static std::string GetStackTrace(PEXCEPTION_POINTERS exceptionInfo);
    static void GenerateMiniDump(PEXCEPTION_POINTERS exceptionInfo, const std::string& dumpPath);
};

#endif
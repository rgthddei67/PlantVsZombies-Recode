#pragma once
#ifndef _CRASH_HANDLER_H
#define _CRASH_HANDLER_H

#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

class CrashHandler {
public:
	// 初始化崩溃处理器(VEH)。非 Windows 平台为空操作。
	static void Initialize();

	// 清理崩溃处理器。非 Windows 平台为空操作。
	static void Cleanup();

#if defined(_WIN32)
private:
	static PVOID vehHandle;
	static LONG64 lastUACTime;

	static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS exceptionInfo);
	static void HandleCrash(PEXCEPTION_POINTERS exceptionInfo);
	static void HandleStackOverflowMinimal(PEXCEPTION_POINTERS exceptionInfo);
	static bool IsCrashException(DWORD exceptionCode);
	static std::string GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo);
	static void ShowCrashDialog(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath);
	static std::string GetExceptionCodeString(DWORD exceptionCode);
	static std::string GetStackTrace(PEXCEPTION_POINTERS exceptionInfo);
	static std::string GetModuleName(HMODULE module);
	static bool IsUACByTimeWindow(PEXCEPTION_POINTERS exceptionInfo);
#endif
};

#endif // _CRASH_HANDLER_H

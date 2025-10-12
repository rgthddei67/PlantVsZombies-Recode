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
    // ��ʼ������������
    static void Initialize();

    // �������������
    static void Cleanup();

private:
    // VEH �쳣������
    static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS exceptionInfo);

    // ���������������
    static void HandleCrash(PEXCEPTION_POINTERS exceptionInfo);

    // �ǲ��Ǳ����쳣
    static bool IsCrashException(DWORD exceptionCode);

    // ���ɱ�������
    static std::string GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo);

    // ��ʾ�����Ի���
    static void ShowCrashDialog(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath);

    // ��ȡ�쳣���������
    static std::string GetExceptionCodeString(DWORD exceptionCode);

    // ��ȡջ������Ϣ
    static std::string GetStackTrace(PEXCEPTION_POINTERS exceptionInfo);

    // ��ȡģ������
    static std::string GetModuleName(HMODULE module);

    static bool IsUACByTimeWindow(PEXCEPTION_POINTERS exceptionInfo);
};

#endif
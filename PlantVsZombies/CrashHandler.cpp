#include "CrashHandler.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <Psapi.h>

#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "Shell32.lib")

LPTOP_LEVEL_EXCEPTION_FILTER CrashHandler::previousFilter = nullptr;

void CrashHandler::Initialize() {
    // �����쳣������
    previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilter);

    std::cout << "�����������ѳ�ʼ��" << std::endl;
}

void CrashHandler::Cleanup() {
    // �ָ�֮ǰ���쳣������
    if (previousFilter) {
        SetUnhandledExceptionFilter(previousFilter);
    }
    std::cout << "����������������" << std::endl;
}

LONG WINAPI CrashHandler::UnhandledExceptionFilter(PEXCEPTION_POINTERS exceptionInfo) {
    std::cout << "��⵽δ�����쳣���������ɱ�������..." << std::endl;

    // ����ʱ���
    auto now = std::time(nullptr);
    char timestamp[64];

    std::tm timeInfo;
    localtime_s(&timeInfo, &now);
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeInfo);

    // ���ɱ����ļ���
    std::string reportPath = "crash_report_" + std::string(timestamp) + ".txt";
    std::string dumpPath = "crash_dump_" + std::string(timestamp) + ".dmp";

    try {
        // ������ϸ��������
        GenerateCrashReport(exceptionInfo, reportPath);

        // �����ڴ�ת���ļ�
        GenerateMiniDump(exceptionInfo, dumpPath);

        // ׼��������Ϣ
        std::stringstream errorMsg;
        errorMsg << "����������һ�����ش�����Ҫ�رա�\n\n";
        errorMsg << "��������: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode) << "\n";
        errorMsg << "�����ַ: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionAddress << "\n\n";
        errorMsg << "��ϸ�ı�����Ϣ�ѱ��浽:\n" << reportPath << "\n\n";
        errorMsg << "�뽫���ļ����͸�������Ա�԰���������⡣";

        // ��ʾ����Ի���
        ShowCrashDialog(errorMsg.str(), reportPath);

    }
    catch (...) {
        // ���������������Ҳʧ���ˣ���ʾ�����Ϣ
        MessageBoxA(nullptr,
            "�����������ش����޷�������ϸ���档",
            "��������",
            MB_ICONERROR | MB_OK);
    }

    // ��ֹ����
    return EXCEPTION_EXECUTE_HANDLER;
}

// ����С��ת���ļ�����
void CrashHandler::GenerateMiniDump(PEXCEPTION_POINTERS exceptionInfo, const std::string& dumpPath) {
    HANDLE hFile = CreateFileA(dumpPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ExceptionPointers = exceptionInfo;
        dumpInfo.ClientPointers = FALSE;

        // ����С��ת��
        MiniDumpWriteDump(GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpNormal,
            &dumpInfo,
            nullptr,
            nullptr);

        CloseHandle(hFile);
    }
}

std::string CrashHandler::GetExceptionCodeString(DWORD exceptionCode) {
    switch (exceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:         return "����Υ�� (EXCEPTION_ACCESS_VIOLATION)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "����Խ�� (EXCEPTION_ARRAY_BOUNDS_EXCEEDED)";
    case EXCEPTION_BREAKPOINT:               return "�ϵ��쳣 (EXCEPTION_BREAKPOINT)";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "����δ���� (EXCEPTION_DATATYPE_MISALIGNMENT)";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "�������쳣������ (EXCEPTION_FLT_DENORMAL_OPERAND)";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "���������� (EXCEPTION_FLT_DIVIDE_BY_ZERO)";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "����������ȷ��� (EXCEPTION_FLT_INEXACT_RESULT)";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "��������Ч���� (EXCEPTION_FLT_INVALID_OPERATION)";
    case EXCEPTION_FLT_OVERFLOW:             return "���������� (EXCEPTION_FLT_OVERFLOW)";
    case EXCEPTION_FLT_STACK_CHECK:          return "������ջ���ʧ�� (EXCEPTION_FLT_STACK_CHECK)";
    case EXCEPTION_FLT_UNDERFLOW:            return "���������� (EXCEPTION_FLT_UNDERFLOW)";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "�Ƿ�ָ�� (EXCEPTION_ILLEGAL_INSTRUCTION)";
    case EXCEPTION_IN_PAGE_ERROR:            return "ҳ����� (EXCEPTION_IN_PAGE_ERROR)";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "�������� (EXCEPTION_INT_DIVIDE_BY_ZERO)";
    case EXCEPTION_INT_OVERFLOW:             return "������� (EXCEPTION_INT_OVERFLOW)";
    case EXCEPTION_INVALID_DISPOSITION:      return "��Ч���� (EXCEPTION_INVALID_DISPOSITION)";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "���ɼ����쳣 (EXCEPTION_NONCONTINUABLE_EXCEPTION)";
    case EXCEPTION_PRIV_INSTRUCTION:         return "��Ȩָ�� (EXCEPTION_PRIV_INSTRUCTION)";
    case EXCEPTION_SINGLE_STEP:              return "����ִ�� (EXCEPTION_SINGLE_STEP)";
    case EXCEPTION_STACK_OVERFLOW:           return "ջ��� (EXCEPTION_STACK_OVERFLOW)";
    default:                                 return "δ֪�쳣 (0x" + std::to_string(exceptionCode) + ")";
    }
}

void CrashHandler::GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath) {
    std::ofstream report(reportPath);
    if (!report.is_open()) {
        return;
    }

    auto now = std::time(nullptr);
    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &now);

    report << "=== ����������� ===\n";
    report << "����ʱ��: " << timeStr;
    report << "�쳣����: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode) << "\n";
    report << "�쳣��ַ: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
    report << "�쳣����: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << "\n";
    report << "�߳�ID: " << std::dec << GetCurrentThreadId() << "\n";
    report << "����ID: " << std::dec << GetCurrentProcessId() << "\n\n";

    // �Ĵ�����Ϣ
    report << "=== �Ĵ���״̬ ===\n";
    PCONTEXT context = exceptionInfo->ContextRecord;
    report << "RAX: 0x" << std::hex << context->Rax << "\n";
    report << "RBX: 0x" << std::hex << context->Rbx << "\n";
    report << "RCX: 0x" << std::hex << context->Rcx << "\n";
    report << "RDX: 0x" << std::hex << context->Rdx << "\n";
    report << "RSI: 0x" << std::hex << context->Rsi << "\n";
    report << "RDI: 0x" << std::hex << context->Rdi << "\n";
    report << "RBP: 0x" << std::hex << context->Rbp << "\n";
    report << "R8: 0x" << std::hex << context->R8 << "\n";
    report << "R9: 0x" << std::hex << context->R9 << "\n";
    report << "R10: 0x" << std::hex << context->R10 << "\n";
    report << "R11: 0x" << std::hex << context->R11 << "\n";
    report << "R12: 0x" << std::hex << context->R12 << "\n";
    report << "R13: 0x" << std::hex << context->R13 << "\n";
    report << "R14: 0x" << std::hex << context->R14 << "\n";
    report << "R15: 0x" << std::hex << context->R15 << "\n";
    report << "RSP: 0x" << std::hex << context->Rsp << "\n";
    report << "RIP: 0x" << std::hex << context->Rip << "\n\n";

    // ջ����
    report << "=== ջ���� ===\n";
    report << GetStackTrace(exceptionInfo) << "\n";

    // ģ����Ϣ
    report << "=== ���ص�ģ�� ===\n";
    HMODULE modules[1024];
    DWORD needed;
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
        for (DWORD i = 0; i < (needed / sizeof(HMODULE)); i++) {
            char moduleName[MAX_PATH];
            if (GetModuleFileNameA(modules[i], moduleName, MAX_PATH)) {
                report << moduleName << "\n";
            }
        }
    }

    report.close();
}

std::string CrashHandler::GetStackTrace(PEXCEPTION_POINTERS exceptionInfo) {
    std::stringstream stackTrace;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    // ��ʼ�����Ŵ���
    SymInitialize(process, nullptr, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    STACKFRAME64 stackFrame = {};
    stackFrame.AddrPC.Offset = exceptionInfo->ContextRecord->Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = exceptionInfo->ContextRecord->Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = exceptionInfo->ContextRecord->Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

    for (int i = 0; i < 32; i++) {
        if (!StackWalk64(machineType, process, thread, &stackFrame, exceptionInfo->ContextRecord,
            nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }

        if (stackFrame.AddrPC.Offset == 0) {
            break;
        }

        stackTrace << "[" << i << "] ";

        // ��ȡģ����
        HMODULE module = nullptr;
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCTSTR)stackFrame.AddrPC.Offset, &module);
        if (module) {
            stackTrace << GetModuleName(module) << "!";
        }

        stackTrace << "0x" << std::hex << stackFrame.AddrPC.Offset;

        // ���Ի�ȡ������
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbolBuffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacement, symbol)) {
            stackTrace << " " << symbol->Name;
        }

        stackTrace << "\n";
    }

    SymCleanup(process);
    return stackTrace.str();
}

std::string CrashHandler::GetModuleName(HMODULE module) {
    char moduleName[MAX_PATH];
    if (GetModuleFileNameA(module, moduleName, MAX_PATH)) {
        std::string fullPath(moduleName);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return fullPath.substr(lastSlash + 1);
        }
        return fullPath;
    }
    return "Unknown";
}

void CrashHandler::ShowCrashDialog(const std::string& errorMessage, const std::string& reportPath) {
    std::string fullMessage = errorMessage + "\n\n���\"ȷ��\"�رճ��򣬵��\"ȡ��\"�鿴�������档";

    int result = MessageBoxA(nullptr, fullMessage.c_str(), "�������",
        MB_ICONERROR | MB_OKCANCEL | MB_DEFBUTTON1);

    if (result == IDCANCEL) {
        ShellExecuteA(nullptr, "open", reportPath.c_str(), nullptr, nullptr, SW_SHOW);
    }
}
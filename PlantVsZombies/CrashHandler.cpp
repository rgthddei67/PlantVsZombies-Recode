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
    // 设置异常处理器
    previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilter);

    std::cout << "崩溃处理器已初始化" << std::endl;
}

void CrashHandler::Cleanup() {
    // 恢复之前的异常过滤器
    if (previousFilter) {
        SetUnhandledExceptionFilter(previousFilter);
    }
    std::cout << "崩溃处理器已清理" << std::endl;
}

LONG WINAPI CrashHandler::UnhandledExceptionFilter(PEXCEPTION_POINTERS exceptionInfo) {
    std::cout << "检测到未处理异常，正在生成崩溃报告..." << std::endl;

    // 生成时间戳
    auto now = std::time(nullptr);
    char timestamp[64];

    std::tm timeInfo;
    localtime_s(&timeInfo, &now);
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeInfo);

    // 生成报告文件名
    std::string reportPath = "crash_report_" + std::string(timestamp) + ".txt";
    std::string dumpPath = "crash_dump_" + std::string(timestamp) + ".dmp";

    try {
        // 生成详细崩溃报告
        GenerateCrashReport(exceptionInfo, reportPath);

        // 生成内存转储文件
        GenerateMiniDump(exceptionInfo, dumpPath);

        // 准备错误信息
        std::stringstream errorMsg;
        errorMsg << "程序遇到了一个严重错误，需要关闭。\n\n";
        errorMsg << "错误类型: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode) << "\n";
        errorMsg << "错误地址: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionAddress << "\n\n";
        errorMsg << "详细的崩溃信息已保存到:\n" << reportPath << "\n\n";
        errorMsg << "请将此文件发送给开发人员以帮助解决问题。";

        // 显示错误对话框
        ShowCrashDialog(errorMsg.str(), reportPath);

    }
    catch (...) {
        // 如果崩溃报告生成也失败了，显示最简信息
        MessageBoxA(nullptr,
            "程序发生了严重错误，无法生成详细报告。",
            "致命错误",
            MB_ICONERROR | MB_OK);
    }

    // 终止程序
    return EXCEPTION_EXECUTE_HANDLER;
}

// 生成小型转储文件函数
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

        // 生成小型转储
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
    case EXCEPTION_ACCESS_VIOLATION:         return "访问违规 (EXCEPTION_ACCESS_VIOLATION)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "数组越界 (EXCEPTION_ARRAY_BOUNDS_EXCEEDED)";
    case EXCEPTION_BREAKPOINT:               return "断点异常 (EXCEPTION_BREAKPOINT)";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "数据未对齐 (EXCEPTION_DATATYPE_MISALIGNMENT)";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "浮点数异常操作数 (EXCEPTION_FLT_DENORMAL_OPERAND)";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "浮点数除零 (EXCEPTION_FLT_DIVIDE_BY_ZERO)";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "浮点数不精确结果 (EXCEPTION_FLT_INEXACT_RESULT)";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "浮点数无效操作 (EXCEPTION_FLT_INVALID_OPERATION)";
    case EXCEPTION_FLT_OVERFLOW:             return "浮点数上溢 (EXCEPTION_FLT_OVERFLOW)";
    case EXCEPTION_FLT_STACK_CHECK:          return "浮点数栈检查失败 (EXCEPTION_FLT_STACK_CHECK)";
    case EXCEPTION_FLT_UNDERFLOW:            return "浮点数下溢 (EXCEPTION_FLT_UNDERFLOW)";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "非法指令 (EXCEPTION_ILLEGAL_INSTRUCTION)";
    case EXCEPTION_IN_PAGE_ERROR:            return "页面错误 (EXCEPTION_IN_PAGE_ERROR)";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "整数除零 (EXCEPTION_INT_DIVIDE_BY_ZERO)";
    case EXCEPTION_INT_OVERFLOW:             return "整数溢出 (EXCEPTION_INT_OVERFLOW)";
    case EXCEPTION_INVALID_DISPOSITION:      return "无效处置 (EXCEPTION_INVALID_DISPOSITION)";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "不可继续异常 (EXCEPTION_NONCONTINUABLE_EXCEPTION)";
    case EXCEPTION_PRIV_INSTRUCTION:         return "特权指令 (EXCEPTION_PRIV_INSTRUCTION)";
    case EXCEPTION_SINGLE_STEP:              return "单步执行 (EXCEPTION_SINGLE_STEP)";
    case EXCEPTION_STACK_OVERFLOW:           return "栈溢出 (EXCEPTION_STACK_OVERFLOW)";
    default:                                 return "未知异常 (0x" + std::to_string(exceptionCode) + ")";
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

    report << "=== 程序崩溃报告 ===\n";
    report << "生成时间: " << timeStr;
    report << "异常类型: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode) << "\n";
    report << "异常地址: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
    report << "异常代码: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << "\n";
    report << "线程ID: " << std::dec << GetCurrentThreadId() << "\n";
    report << "进程ID: " << std::dec << GetCurrentProcessId() << "\n\n";

    // 寄存器信息
    report << "=== 寄存器状态 ===\n";
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

    // 栈跟踪
    report << "=== 栈跟踪 ===\n";
    report << GetStackTrace(exceptionInfo) << "\n";

    // 模块信息
    report << "=== 加载的模块 ===\n";
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

    // 初始化符号处理
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

        // 获取模块名
        HMODULE module = nullptr;
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCTSTR)stackFrame.AddrPC.Offset, &module);
        if (module) {
            stackTrace << GetModuleName(module) << "!";
        }

        stackTrace << "0x" << std::hex << stackFrame.AddrPC.Offset;

        // 尝试获取符号名
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
    std::string fullMessage = errorMessage + "\n\n点击\"确定\"关闭程序，点击\"取消\"查看崩溃报告。";

    int result = MessageBoxA(nullptr, fullMessage.c_str(), "程序崩溃",
        MB_ICONERROR | MB_OKCANCEL | MB_DEFBUTTON1);

    if (result == IDCANCEL) {
        ShellExecuteA(nullptr, "open", reportPath.c_str(), nullptr, nullptr, SW_SHOW);
    }
}
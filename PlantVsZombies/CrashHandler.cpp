#include "CrashHandler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <DbgHelp.h>
#include <shellapi.h>
#include <psapi.h>
#include <TlHelp32.h>
#include <algorithm>

#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "Shell32.lib")

PVOID CrashHandler::vehHandle = nullptr;
LONG64 CrashHandler::lastUACTime = 0;

void CrashHandler::Initialize() {
    std::cout << "安装向量化异常处理器..." << std::endl;
    ULONG stackGuarantee = 4 * 1024; // 保留4KB栈空间
    SetThreadStackGuarantee(&stackGuarantee);
    // 安装 VEH 作为第一个异常处理器
    vehHandle = AddVectoredExceptionHandler(1, VectoredExceptionHandler);

    if (vehHandle) {
        std::cout << "向量化异常处理器安装成功" << std::endl;
    }
    else {
        std::cerr << "向量化异常处理器安装失败" << std::endl;
    }
}

void CrashHandler::Cleanup() {
    if (vehHandle) {
        RemoveVectoredExceptionHandler(vehHandle);
        vehHandle = nullptr;
        std::cout << "向量化异常处理器已卸载" << std::endl;
    }
}

LONG WINAPI CrashHandler::VectoredExceptionHandler(PEXCEPTION_POINTERS exceptionInfo) {
    if (!exceptionInfo) return EXCEPTION_CONTINUE_SEARCH;
    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

    if (IsUACByTimeWindow(exceptionInfo))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (exceptionCode == EXCEPTION_STACK_OVERFLOW) {
        HandleStackOverflowMinimal(exceptionInfo);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    // 排除调试相关的异常
    switch (exceptionCode)
    {
    case EXCEPTION_BREAKPOINT:                    // 断点
    case EXCEPTION_SINGLE_STEP:                   // 单步
    case 0x4001000a:                              // DBG_PRINTEXCEPTION_C (调试输出)
    case 0x40010006:                              // DBG_PRINTEXCEPTION_WIDE_C (宽字符调试输出)
    case 0x406D1388:                              // 设置线程名称异常
        return EXCEPTION_CONTINUE_SEARCH;
    case 0xE06D7363:
        return EXCEPTION_CONTINUE_SEARCH;         // C++ 异常

    default:
        if (IsCrashException(exceptionCode))
        {
            HandleCrash(exceptionInfo);
            return EXCEPTION_EXECUTE_HANDLER;
        }
        else
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
}

void CrashHandler::HandleStackOverflowMinimal(PEXCEPTION_POINTERS exceptionInfo) {
    // 使用静态缓冲区，避免栈分配
    static char buffer[512];

    // 简单的错误信息
    const char* errorMsg =
        "Stack overflow occurred.\n"
        "The program will terminate immediately.";

    // 直接写入文件，避免复杂操作
    HANDLE hFile = CreateFileA(
        "stack_overflow_minimal.txt",
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    printf("\a");
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hFile, errorMsg, strlen(errorMsg), &written, nullptr);

        // 记录异常地址
        if (exceptionInfo && exceptionInfo->ExceptionRecord) {
            std::snprintf(buffer, sizeof(buffer),
                "\n\nException Address: 0x%p\nRSP: 0x%p",
                exceptionInfo->ExceptionRecord->ExceptionAddress, 
                exceptionInfo->ContextRecord->Rsp);
            WriteFile(hFile, buffer, strlen(buffer), &written, nullptr);
        }

        CloseHandle(hFile);
    }

	TerminateProcess(GetCurrentProcess(), -114514);
}

bool CrashHandler::IsCrashException(DWORD exceptionCode) {
    switch (exceptionCode) {
        // 内存访问相关
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_STACK_INVALID:

        // 算术运算相关
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_DENORMAL_OPERAND:

        // 指令相关
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:

        // 数组和数据类型
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:

        // 其他严重错误
    case EXCEPTION_INVALID_HANDLE:
        return true;

    default:
        return false;
    }
}

void CrashHandler::HandleCrash(PEXCEPTION_POINTERS exceptionInfo) {
    try {
        std::string reportPath = GenerateCrashReport(exceptionInfo);
        ShowCrashDialog(exceptionInfo, reportPath);
    }
    catch (...) {
        // 如果崩溃报告生成失败，显示最简单的错误信息
        MessageBoxA(nullptr,
            "程序发生了严重错误，无法生成详细报告。",
            "致命错误",
            MB_ICONERROR | MB_OK);
    }

    TerminateProcess(GetCurrentProcess(), 1);
}

std::string CrashHandler::GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo) {
    // 生成时间戳文件名
    auto now = std::time(nullptr);
    char timestamp[64];
    std::tm timeInfo;
    localtime_s(&timeInfo, &now);
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeInfo);

    std::string reportPath = "crash_report_" + std::string(timestamp) + ".txt";

    std::ofstream report(reportPath);
    if (!report.is_open()) {
        return reportPath;
    }

    // 格式化时间字符串
    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &now);

    report << "=== 程序崩溃报告 ===\n";
    report << "生成时间: " << timeStr;
    report << "异常类型: " << GetExceptionCodeString
    (exceptionInfo->ExceptionRecord->ExceptionCode)
        << "(0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << ")\n";
    report << "异常地址: 0x" << std::hex << (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
    report << "线程ID: " << std::dec << GetCurrentThreadId() << "\n";
    report << "进程ID: " << std::dec << GetCurrentProcessId() << "\n\n";

    // 寄存器信息
    if (exceptionInfo->ContextRecord) {
        report << "=== 寄存器状态 ===\n";
        PCONTEXT ctx = exceptionInfo->ContextRecord;

#ifdef _WIN64
        report << "RAX: 0x" << std::hex << ctx->Rax << "\n";
        report << "RBX: 0x" << std::hex << ctx->Rbx << "\n";
        report << "RCX: 0x" << std::hex << ctx->Rcx << "\n";
        report << "RDX: 0x" << std::hex << ctx->Rdx << "\n";
        report << "RSI: 0x" << std::hex << ctx->Rsi << "\n";
        report << "RDI: 0x" << std::hex << ctx->Rdi << "\n";
        report << "RBP: 0x" << std::hex << ctx->Rbp << "\n";
        report << "RSP: 0x" << std::hex << ctx->Rsp << "\n";
        report << "RIP: 0x" << std::hex << ctx->Rip << "\n";
        report << "R8: 0x" << std::hex << ctx->R8 << "\n";
        report << "R9: 0x" << std::hex << ctx->R9 << "\n";
        report << "R10: 0x" << std::hex << ctx->R10 << "\n";
        report << "R11: 0x" << std::hex << ctx->R11 << "\n";
        report << "R12: 0x" << std::hex << ctx->R12 << "\n";
        report << "R13: 0x" << std::hex << ctx->R13 << "\n";
        report << "R14: 0x" << std::hex << ctx->R14 << "\n";
        report << "R15: 0x" << std::hex << ctx->R15 << "\n";
#else
        report << "EAX: 0x" << std::hex << ctx->Eax << "\n";
        report << "EBX: 0x" << std::hex << ctx->Ebx << "\n";
        report << "ECX: 0x" << std::hex << ctx->Ecx << "\n";
        report << "EDX: 0x" << std::hex << ctx->Edx << "\n";
        report << "ESI: 0x" << std::hex << ctx->Esi << "\n";
        report << "EDI: 0x" << std::hex << ctx->Edi << "\n";
        report << "EBP: 0x" << std::hex << ctx->Ebp << "\n";
        report << "ESP: 0x" << std::hex << ctx->Esp << "\n";
        report << "EIP: 0x" << std::hex << ctx->Eip << "\n";
#endif
        report << "\n";
    }

    // 栈跟踪
    report << "=== 栈跟踪 ===\n";
    report << GetStackTrace(exceptionInfo) << "\n";

    // 加载的模块信息
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
    return reportPath;
}

void CrashHandler::ShowCrashDialog(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath) {
    if (!exceptionInfo) {
        MessageBoxA(nullptr, "发生未知错误", "程序崩溃", MB_ICONERROR | MB_OK);
        return;
    }

    // 构建详细的错误信息
    std::stringstream errorMsg;
    errorMsg << "程序遇到了严重错误需要关闭\n\n";
    errorMsg << "错误类型: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode) << "\n";
    errorMsg << "错误地址: 0x" << std::hex << (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress << "\n\n";

    // 添加寄存器信息
    if (exceptionInfo->ContextRecord) {
        errorMsg << "以下是部分关键寄存器信息，完整请看崩溃报告。\n";
        errorMsg << "=== 寄存器信息 ===\n";
        PCONTEXT ctx = exceptionInfo->ContextRecord;

#ifdef _WIN64
        errorMsg << "RAX: 0x" << std::hex << ctx->Rax << "\n";
        errorMsg << "RBX: 0x" << std::hex << ctx->Rbx << "\n";
        errorMsg << "RCX: 0x" << std::hex << ctx->Rcx << "\n";
        errorMsg << "RDX: 0x" << std::hex << ctx->Rdx << "\n";
        errorMsg << "RSI: 0x" << std::hex << ctx->Rsi << "\n";
        errorMsg << "RDI: 0x" << std::hex << ctx->Rdi << "\n";
        errorMsg << "RBP: 0x" << std::hex << ctx->Rbp << "\n";
        errorMsg << "RSP: 0x" << std::hex << ctx->Rsp << "\n";
        errorMsg << "RIP: 0x" << std::hex << ctx->Rip << "\n";
#else
        errorMsg << "EAX: 0x" << std::hex << ctx->Eax << "\n";
        errorMsg << "EBX: 0x" << std::hex << ctx->Ebx << "\n";
        errorMsg << "ECX: 0x" << std::hex << ctx->Ecx << "\n";
        errorMsg << "EDX: 0x" << std::hex << ctx->Edx << "\n";
        errorMsg << "ESI: 0x" << std::hex << ctx->Esi << "\n";
        errorMsg << "EDI: 0x" << std::hex << ctx->Edi << "\n";
        errorMsg << "EBP: 0x" << std::hex << ctx->Ebp << "\n";
        errorMsg << "ESP: 0x" << std::hex << ctx->Esp << "\n";
        errorMsg << "EIP: 0x" << std::hex << ctx->Eip << "\n";
#endif
        errorMsg << "\n";
    }

    errorMsg << "崩溃报告已保存到:\n" << reportPath << "\n\n";
    errorMsg << "请将此文件发送给开发人员以帮助解决问题。\n";
    errorMsg << "点击\"确定\"关闭程序。";

    MessageBoxA(nullptr, errorMsg.str().c_str(), "程序崩溃", MB_ICONERROR | MB_OK);
}

std::string CrashHandler::GetExceptionCodeString(DWORD exceptionCode) {
    switch (exceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:         return "访问违规";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "数组越界";
    case EXCEPTION_BREAKPOINT:               return "断点";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "数据未对齐";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "浮点数异常操作数";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "浮点数除零";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "浮点数不精确结果";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "浮点数无效操作";
    case EXCEPTION_FLT_OVERFLOW:             return "浮点数上溢";
    case EXCEPTION_FLT_STACK_CHECK:          return "浮点数栈检查失败";
    case EXCEPTION_FLT_UNDERFLOW:            return "浮点数下溢";
    case EXCEPTION_GUARD_PAGE:               return "保护页违规";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "非法指令";
    case EXCEPTION_IN_PAGE_ERROR:            return "页面错误";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "整数除零";
    case EXCEPTION_INT_OVERFLOW:             return "整数溢出";
    case EXCEPTION_INVALID_DISPOSITION:      return "无效处置";
    case EXCEPTION_INVALID_HANDLE:           return "无效句柄";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "不可继续异常";
    case EXCEPTION_PRIV_INSTRUCTION:         return "特权指令";
    case EXCEPTION_SINGLE_STEP:              return "单步执行";
    case EXCEPTION_STACK_OVERFLOW:           return "栈溢出";
    case EXCEPTION_STACK_INVALID:            return "栈无效";
    case 0x4001000a:                         return "调试输出异常";
    case 0x40010006:                         return "宽字符调试输出异常";
    case 0x406D1388:                         return "设置线程名称异常";
    case 0xE06D7363:                         return "C++异常";
    default:
        return "未知异常 (0x" + std::to_string(exceptionCode) + ")";
    }
}

std::string CrashHandler::GetStackTrace(PEXCEPTION_POINTERS exceptionInfo) {
    std::stringstream stackTrace;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    // 初始化符号处理
    SymInitialize(process, nullptr, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    STACKFRAME64 stackFrame = {};

    if (exceptionInfo && exceptionInfo->ContextRecord) {
#ifdef _WIN64
        stackFrame.AddrPC.Offset = exceptionInfo->ContextRecord->Rip;
        stackFrame.AddrFrame.Offset = exceptionInfo->ContextRecord->Rbp;
        stackFrame.AddrStack.Offset = exceptionInfo->ContextRecord->Rsp;
#else
        stackFrame.AddrPC.Offset = exceptionInfo->ContextRecord->Eip;
        stackFrame.AddrFrame.Offset = exceptionInfo->ContextRecord->Ebp;
        stackFrame.AddrStack.Offset = exceptionInfo->ContextRecord->Esp;
#endif
    }

    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

#ifdef _WIN64
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#else
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
#endif

    for (int i = 0; i < 32; i++) {
        if (!StackWalk64(machineType, process, thread, &stackFrame,
            exceptionInfo ? exceptionInfo->ContextRecord : nullptr,
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
            if (displacement != 0) {
                stackTrace << "+0x" << std::hex << displacement;
            }
        }

        stackTrace << "\n";
    }

    SymCleanup(process);

    std::string result = stackTrace.str();
    if (result.empty()) {
        return "无法获取栈跟踪信息";
    }

    return result;
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

bool CrashHandler::IsUACByTimeWindow(PEXCEPTION_POINTERS exceptionInfo) {
    if (!exceptionInfo) return false;

    LONG64 currentTime = GetTickCount64();

    // 如果在最近3秒内检测到过UAC，忽略所有异常
    if (currentTime - lastUACTime < 3000) {
        std::cout << "时间窗口内，忽略异常 (距离上次UAC: "
            << (currentTime - lastUACTime) << "ms)" << std::endl;
        return true;
    }

    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

    // 检查是否是典型的UAC异常模式
    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        exceptionCode == EXCEPTION_GUARD_PAGE ||
        exceptionCode == EXCEPTION_BREAKPOINT) {

        HMODULE hModule = nullptr;
        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCTSTR)exceptionInfo->ExceptionRecord->ExceptionAddress, &hModule)) {

            char moduleName[MAX_PATH];
            if (GetModuleFileNameA(hModule, moduleName, MAX_PATH)) {
                std::string modulePath = moduleName;
                std::transform(modulePath.begin(), modulePath.end(), modulePath.begin(), ::tolower);

                // 如果是系统核心模块，认为是UAC
                if (modulePath.find("\\system32\\") != std::string::npos ||
                    modulePath.find("\\syswow64\\") != std::string::npos ||
                    modulePath.find("kernel32.dll") != std::string::npos ||
                    modulePath.find("kernelbase.dll") != std::string::npos ||
                    modulePath.find("ntdll.dll") != std::string::npos) {

                    std::cout << "检测到UAC相关异常，模块: " << modulePath << std::endl;
                    lastUACTime = currentTime;
                    return true;
                }
            }
        }
    }

    return false;
}

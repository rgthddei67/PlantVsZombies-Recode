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
    std::cout << "Installing vectored exception handler..." << std::endl;
    ULONG stackGuarantee = 4 * 1024; // 4KB stack guarantee
    SetThreadStackGuarantee(&stackGuarantee);
    vehHandle = AddVectoredExceptionHandler(1, VectoredExceptionHandler);

    if (vehHandle) {
        std::cout << "Vectored exception handler installed successfully" << std::endl;
    }
    else {
        std::cerr << "Failed to install vectored exception handler" << std::endl;
    }
}

void CrashHandler::Cleanup() {
    if (vehHandle) {
        RemoveVectoredExceptionHandler(vehHandle);
        vehHandle = nullptr;
        std::cout << "Vectored exception handler uninstalled" << std::endl;
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

    // Skip debug-related exceptions
    switch (exceptionCode)
    {
    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
    case 0x4001000a:                              // DBG_PRINTEXCEPTION_C
    case 0x40010006:                              // DBG_PRINTEXCEPTION_WIDE_C
    case 0x406D1388:                              // SetThreadName exception
        return EXCEPTION_CONTINUE_SEARCH;
    case 0xE06D7363:                              // C++ exception
        return EXCEPTION_CONTINUE_SEARCH;

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
    static char buffer[512];

    const char* errorMsg =
        "Stack overflow occurred.\n"
        "The program will terminate immediately.";

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
        // Memory access
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_STACK_INVALID:

        // Arithmetic
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_DENORMAL_OPERAND:

        // Instruction
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:

        // Array and data type
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:

        // Other severe errors
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
        MessageBoxA(nullptr,
            "A serious error occurred and a detailed report could not be generated.",
            "Fatal Error",
            MB_ICONERROR | MB_OK);
    }

    TerminateProcess(GetCurrentProcess(), 1);
}

std::string CrashHandler::GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo) {
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

    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &now);

    report << "=== Crash Report ===\n";
    report << "Time: " << timeStr;
    report << "Exception Code: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode)
        << "(0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << ")\n";
    report << "Exception Address: 0x" << std::hex << (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
    report << "Thread ID: " << std::dec << GetCurrentThreadId() << "\n";
    report << "Process ID: " << std::dec << GetCurrentProcessId() << "\n\n";

    if (exceptionInfo->ContextRecord) {
        report << "=== Register State ===\n";
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

    report << "=== Stack Trace ===\n";
    report << GetStackTrace(exceptionInfo) << "\n";

    report << "=== Loaded Modules ===\n";
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
        MessageBoxA(nullptr, "Unknown error occurred", "Fatal Error", MB_ICONERROR | MB_OK);
        return;
    }

    std::stringstream errorMsg;
    errorMsg << "The program encountered a fatal error and needs to close.\n\n";
    errorMsg << "Exception Code: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode) << "\n";
    errorMsg << "Exception Address: 0x" << std::hex << (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress << "\n\n";

    if (exceptionInfo->ContextRecord) {
        errorMsg << "Below are some key register values. See the crash report for full details.\n";
        errorMsg << "=== Register Information ===\n";
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

    errorMsg << "Crash report saved to: " << reportPath << "\n\n";
    errorMsg << "Please send this file to the developer for troubleshooting.\n";
    errorMsg << "Click 'OK' to close the program.";

    MessageBoxA(nullptr, errorMsg.str().c_str(), "Fatal Error", MB_ICONERROR | MB_OK);
}

std::string CrashHandler::GetExceptionCodeString(DWORD exceptionCode) {
    switch (exceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:         return u8"Access Violation";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return u8"Array Bounds Exceeded";
    case EXCEPTION_BREAKPOINT:               return u8"Breakpoint";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return u8"Data Misalignment";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return u8"Floating-point Denormal Operand";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return u8"Floating-point Divide by Zero";
    case EXCEPTION_FLT_INEXACT_RESULT:       return u8"Floating-point Inexact Result";
    case EXCEPTION_FLT_INVALID_OPERATION:    return u8"Floating-point Invalid Operation";
    case EXCEPTION_FLT_OVERFLOW:             return u8"Floating-point Overflow";
    case EXCEPTION_FLT_STACK_CHECK:          return u8"Floating-point Stack Check";
    case EXCEPTION_FLT_UNDERFLOW:            return u8"Floating-point Underflow";
    case EXCEPTION_GUARD_PAGE:               return u8"Guard Page Violation";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return u8"Illegal Instruction";
    case EXCEPTION_IN_PAGE_ERROR:            return u8"Page Error";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return u8"Integer Divide by Zero";
    case EXCEPTION_INT_OVERFLOW:             return u8"Integer Overflow";
    case EXCEPTION_INVALID_DISPOSITION:      return u8"Invalid Disposition";
    case EXCEPTION_INVALID_HANDLE:           return u8"Invalid Handle";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return u8"Noncontinuable Exception";
    case EXCEPTION_PRIV_INSTRUCTION:         return u8"Privileged Instruction";
    case EXCEPTION_SINGLE_STEP:              return u8"Single Step";
    case EXCEPTION_STACK_OVERFLOW:           return u8"Stack Overflow";
    case EXCEPTION_STACK_INVALID:            return u8"Stack Invalid";
    case 0x4001000a:                         return u8"Debug Output Exception";
    case 0x40010006:                         return u8"Wide Debug Output Exception";
    case 0x406D1388:                         return u8"Set Thread Name Exception";
    case 0xE06D7363:                         return u8"C++ Exception";
    default:
        return u8"Unknown Exception (0x" + std::to_string(exceptionCode) + ")";
    }
}

std::string CrashHandler::GetStackTrace(PEXCEPTION_POINTERS exceptionInfo) {
    std::stringstream stackTrace;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

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

        HMODULE module = nullptr;
        GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCTSTR)stackFrame.AddrPC.Offset, &module);
        if (module) {
            stackTrace << GetModuleName(module) << "!";
        }

        stackTrace << "0x" << std::hex << stackFrame.AddrPC.Offset;

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
        return "Unable to retrieve stack trace";
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

    if (currentTime - lastUACTime < 3000) {
        std::cout << "Within time window, ignoring exception (time since last UAC: "
            << (currentTime - lastUACTime) << "ms)" << std::endl;
        return true;
    }

    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

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

                if (modulePath.find("\\system32\\") != std::string::npos ||
                    modulePath.find("\\syswow64\\") != std::string::npos ||
                    modulePath.find("kernel32.dll") != std::string::npos ||
                    modulePath.find("kernelbase.dll") != std::string::npos ||
                    modulePath.find("ntdll.dll") != std::string::npos) {

                    std::cout << "Detected UAC-related exception, module: " << modulePath << std::endl;
                    lastUACTime = currentTime;
                    return true;
                }
            }
        }
    }

    return false;
}
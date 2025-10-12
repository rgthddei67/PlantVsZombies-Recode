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
    std::cout << "��װ�������쳣������..." << std::endl;

    // ��װ VEH ��Ϊ��һ���쳣������
    vehHandle = AddVectoredExceptionHandler(1, VectoredExceptionHandler);

    if (vehHandle) {
        std::cout << "�������쳣��������װ�ɹ�" << std::endl;
    }
    else {
        std::cerr << "�������쳣��������װʧ��" << std::endl;
    }
}

void CrashHandler::Cleanup() {
    if (vehHandle) {
        RemoveVectoredExceptionHandler(vehHandle);
        vehHandle = nullptr;
        std::cout << "�������쳣��������ж��" << std::endl;
    }
}

LONG WINAPI CrashHandler::VectoredExceptionHandler(PEXCEPTION_POINTERS exceptionInfo) {
    if (!exceptionInfo) return EXCEPTION_CONTINUE_SEARCH;
    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

    if (IsUACByTimeWindow(exceptionInfo)) 
    {
        return EXCEPTION_CONTINUE_SEARCH;
	}
    // �ų�������ص��쳣
    switch (exceptionCode) 
    {
        case EXCEPTION_BREAKPOINT:                    // �ϵ�
        case EXCEPTION_SINGLE_STEP:                   // ����
        case 0x4001000a:                              // DBG_PRINTEXCEPTION_C (�������)
        case 0x40010006:                              // DBG_PRINTEXCEPTION_WIDE_C (���ַ��������)
        case 0x406D1388:                              // �����߳������쳣
            return EXCEPTION_CONTINUE_SEARCH;
        case 0xE06D7363:                            
            return EXCEPTION_CONTINUE_SEARCH;         // C++ �쳣

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

bool CrashHandler::IsCrashException(DWORD exceptionCode) {
    switch (exceptionCode) {
        // �ڴ�������
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_STACK_INVALID:

        // �����������
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_DENORMAL_OPERAND:

        // ָ�����
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:

        // �������������
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:

        // �������ش���
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
        // ���������������ʧ�ܣ���ʾ��򵥵Ĵ�����Ϣ
        MessageBoxA(nullptr,
            "�����������ش����޷�������ϸ���档",
            "��������",
            MB_ICONERROR | MB_OK);
    }

    TerminateProcess(GetCurrentProcess(), 1);
}

std::string CrashHandler::GenerateCrashReport(PEXCEPTION_POINTERS exceptionInfo) {
    // ����ʱ����ļ���
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

    // ��ʽ��ʱ���ַ���
    char timeStr[26];
    ctime_s(timeStr, sizeof(timeStr), &now);

    report << "=== ����������� ===\n";
    report << "����ʱ��: " << timeStr;
    report << "�쳣����: " << GetExceptionCodeString
    (exceptionInfo->ExceptionRecord->ExceptionCode)
        << "(0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << ")\n";
    report << "�쳣��ַ: 0x" << std::hex << (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress << "\n";
    report << "�߳�ID: " << std::dec << GetCurrentThreadId() << "\n";
    report << "����ID: " << std::dec << GetCurrentProcessId() << "\n\n";

    // �Ĵ�����Ϣ
    if (exceptionInfo->ContextRecord) {
        report << "=== �Ĵ���״̬ ===\n";
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

    // ջ����
    report << "=== ջ���� ===\n";
    report << GetStackTrace(exceptionInfo) << "\n";

    // ���ص�ģ����Ϣ
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
    return reportPath;
}

void CrashHandler::ShowCrashDialog(PEXCEPTION_POINTERS exceptionInfo, const std::string& reportPath) {
    if (!exceptionInfo) {
        MessageBoxA(nullptr, "����δ֪����", "�������", MB_ICONERROR | MB_OK);
        return;
    }

    // ������ϸ�Ĵ�����Ϣ
    std::stringstream errorMsg;
    errorMsg << "�������������ش�����Ҫ�ر�\n\n";
    errorMsg << "��������: " << GetExceptionCodeString(exceptionInfo->ExceptionRecord->ExceptionCode) << "\n";
    errorMsg << "�����ַ: 0x" << std::hex << (uintptr_t)exceptionInfo->ExceptionRecord->ExceptionAddress << "\n\n";

    // ��ӼĴ�����Ϣ
    if (exceptionInfo->ContextRecord) {
		errorMsg << "�����ǲ��ֹؼ��Ĵ�����Ϣ�������뿴�������档\n";
        errorMsg << "=== �Ĵ�����Ϣ ===\n";
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

    errorMsg << "���������ѱ��浽:\n" << reportPath << "\n\n";
    errorMsg << "�뽫���ļ����͸�������Ա�԰���������⡣\n";
    errorMsg << "���\"ȷ��\"�رճ���";

    MessageBoxA(nullptr, errorMsg.str().c_str(), "�������", MB_ICONERROR | MB_OK);
}

std::string CrashHandler::GetExceptionCodeString(DWORD exceptionCode) {
    switch (exceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:         return "����Υ��";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "����Խ��";
    case EXCEPTION_BREAKPOINT:               return "�ϵ�";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "����δ����";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "�������쳣������";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "����������";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "����������ȷ���";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "��������Ч����";
    case EXCEPTION_FLT_OVERFLOW:             return "����������";
    case EXCEPTION_FLT_STACK_CHECK:          return "������ջ���ʧ��";
    case EXCEPTION_FLT_UNDERFLOW:            return "����������";
    case EXCEPTION_GUARD_PAGE:               return "����ҳΥ��";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "�Ƿ�ָ��";
    case EXCEPTION_IN_PAGE_ERROR:            return "ҳ�����";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "��������";
    case EXCEPTION_INT_OVERFLOW:             return "�������";
    case EXCEPTION_INVALID_DISPOSITION:      return "��Ч����";
    case EXCEPTION_INVALID_HANDLE:           return "��Ч���";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "���ɼ����쳣";
    case EXCEPTION_PRIV_INSTRUCTION:         return "��Ȩָ��";
    case EXCEPTION_SINGLE_STEP:              return "����ִ��";
    case EXCEPTION_STACK_OVERFLOW:           return "ջ���";
    case EXCEPTION_STACK_INVALID:            return "ջ��Ч";
    case 0x4001000a:                         return "��������쳣";
    case 0x40010006:                         return "���ַ���������쳣";
    case 0x406D1388:                         return "�����߳������쳣";
    case 0xE06D7363:                         return "C++�쳣";
    default:
        return "δ֪�쳣 (0x" + std::to_string(exceptionCode) + ")";
    }
}

std::string CrashHandler::GetStackTrace(PEXCEPTION_POINTERS exceptionInfo) {
    std::stringstream stackTrace;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    // ��ʼ�����Ŵ���
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
            if (displacement != 0) {
                stackTrace << "+0x" << std::hex << displacement;
            }
        }

        stackTrace << "\n";
    }

    SymCleanup(process);

    std::string result = stackTrace.str();
    if (result.empty()) {
        return "�޷���ȡջ������Ϣ";
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

    // ��������3���ڼ�⵽��UAC�����������쳣
    if (currentTime - lastUACTime < 3000) {
        std::cout << "ʱ�䴰���ڣ������쳣 (�����ϴ�UAC: "
            << (currentTime - lastUACTime) << "ms)" << std::endl;
        return true;
    }

    DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;

    // ����Ƿ��ǵ��͵�UAC�쳣ģʽ
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

                // �����ϵͳ����ģ�飬��Ϊ��UAC
                if (modulePath.find("\\system32\\") != std::string::npos ||
                    modulePath.find("\\syswow64\\") != std::string::npos ||
                    modulePath.find("kernel32.dll") != std::string::npos ||
                    modulePath.find("kernelbase.dll") != std::string::npos ||
                    modulePath.find("ntdll.dll") != std::string::npos) {

                    std::cout << "��⵽UAC����쳣��ģ��: " << modulePath << std::endl;
                    lastUACTime = currentTime;
                    return true;
                }
            }
        }
    }

    return false;
}

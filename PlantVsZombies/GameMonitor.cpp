#include "GameMonitor.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <psapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "psapi.lib")

// 初始化静态成员
PerformanceMetrics GameMonitor::performance = { 0 };
ULONGLONG GameMonitor::frameCount = 0;
ULONGLONG GameMonitor::lastFrameTime = 0;

void GameMonitor::PrintStackUsage() {
	StackUsageInfo usage = GetStackUsage();
	std::cout << "=== 栈使用情况 ===" << std::endl;
	std::cout << "总保留大小: " << usage.totalReserved / 1024 << " KB" << std::endl;
	std::cout << "已使用: " << usage.used / 1024 << " KB" << std::endl;
	std::cout << "剩余: " << usage.remaining / 1024 << " KB" << std::endl;
	std::cout << "使用率: " << std::fixed << std::setprecision(2) << usage.usagePercent << "%" << std::endl;

	if (usage.usagePercent > 80.0) {
		std::cout << "  警告: 栈使用率较高!" << std::endl;
	}
}

StackUsageInfo GameMonitor::GetStackUsage() {
	StackUsageInfo info = { 0 };
	ULONG_PTR stackLow, stackHigh;
	ULONG_PTR currentStack = (ULONG_PTR)_AddressOfReturnAddress();
	GetCurrentThreadStackLimits(&stackLow, &stackHigh);
	info.totalReserved = stackHigh - stackLow;
	info.remaining = currentStack - stackLow;
	info.used = info.totalReserved - info.remaining;
	info.usagePercent = (info.totalReserved > 0) ?
		(double)info.used / info.totalReserved * 100.0 : 0.0;
	info.stackLow = stackLow;
	info.stackHigh = stackHigh;

	return info;
}

void GameMonitor::PrintHeapUsage() {
	SIZE_T heapUsage = GetProcessHeapUsage();
	SIZE_T privateBytes = GetPrivateBytes();

	std::cout << "=== 堆内存使用情况 ===" << std::endl;
	std::cout << "进程堆使用: " << heapUsage / 1024 << " KB" << std::endl;
	std::cout << "私有字节数: " << privateBytes / 1024 << " KB" << std::endl;

	// 安全地获取默认堆信息
	HANDLE defaultHeap = GetProcessHeap();
	if (defaultHeap) {
		// 使用更安全的方法获取堆信息
		HEAP_SUMMARY heapSummary = { 0 };
		heapSummary.cb = sizeof(HEAP_SUMMARY);

		// 尝试使用HeapSummary（更安全的方法）
		if (HeapSummary(defaultHeap, 0, &heapSummary)) {
			std::cout << "默认堆 - 提交大小: " << heapSummary.cbCommitted / 1024 << " KB" << std::endl;
			std::cout << "默认堆 - 保留大小: " << heapSummary.cbReserved / 1024 << " KB" << std::endl;
			std::cout << "默认堆 - 已分配块: " << heapSummary.cbAllocated << std::endl;
		}
		else {
			// 备用方法：使用HeapLock/HeapWalk，但更安全
			if (HeapLock(defaultHeap)) {
				PROCESS_HEAP_ENTRY entry = { 0 };
				SIZE_T totalHeapSize = 0;
				SIZE_T usedHeapSize = 0;
				DWORD entryCount = 0;
				const DWORD maxEntries = 10000; // 防止无限循环

				// 安全地遍历堆
				while (HeapWalk(defaultHeap, &entry) && entryCount < maxEntries) {
					entryCount++;
					totalHeapSize += entry.cbData;
					if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
						usedHeapSize += entry.cbData;
					}

					// 重置entry.cbData以防止潜在问题
					entry.cbData = 0;
				}

				HeapUnlock(defaultHeap);

				std::cout << "默认堆 - 总大小: " << totalHeapSize / 1024 << " KB" << std::endl;
				std::cout << "默认堆 - 已使用: " << usedHeapSize / 1024 << " KB" << std::endl;
				std::cout << "扫描的堆块数量: " << entryCount << std::endl;
			}
			else {
				std::cout << "无法锁定堆进行检测" << std::endl;
			}
		}
	}
}

SIZE_T GameMonitor::GetProcessHeapUsage() {
	SIZE_T totalHeapSize = 0;

	// 安全地获取堆数量
	DWORD numberOfHeaps = GetProcessHeaps(0, nullptr);
	if (numberOfHeaps > 0) {
		HANDLE* heaps = new (std::nothrow) HANDLE[numberOfHeaps];
		if (heaps) {
			DWORD actualHeaps = GetProcessHeaps(numberOfHeaps, heaps);

			for (DWORD i = 0; i < actualHeaps; i++) {
				// 使用HeapSummary替代HeapWalk，更安全
				HEAP_SUMMARY summary = { 0 };
				summary.cb = sizeof(HEAP_SUMMARY);

				if (HeapSummary(heaps[i], 0, &summary)) {
					totalHeapSize += summary.cbCommitted;
				}
				else {
					// 备用方法：安全地使用HeapLock/HeapWalk
					if (HeapLock(heaps[i])) {
						PROCESS_HEAP_ENTRY entry = { 0 };
						SIZE_T heapSize = 0;
						DWORD entryCount = 0;
						const DWORD maxEntries = 5000;

						while (HeapWalk(heaps[i], &entry) && entryCount < maxEntries) {
							entryCount++;
							if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
								heapSize += entry.cbData;
							}
							entry.cbData = 0; // 重置
						}

						HeapUnlock(heaps[i]);
						totalHeapSize += heapSize;
					}
				}
			}

			delete[] heaps;
		}
	}

	return totalHeapSize;
}

SIZE_T GameMonitor::GetPrivateBytes() {
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if (GetProcessMemoryInfo(GetCurrentProcess(),
		(PROCESS_MEMORY_COUNTERS*)&pmc,
		sizeof(pmc))) {
		return pmc.PrivateUsage;
	}
	return 0;
}

void GameMonitor::PrintSystemMemoryStatus() {
	MemoryStatus status = GetSystemMemoryStatus();

	std::cout << "=== 系统内存状态 ===" << std::endl;
	std::cout << "内存负载: " << status.memoryLoad << "%" << std::endl;
	std::cout << "物理内存 - 总量: " << status.totalPhysical / (1024 * 1024) << " MB" << std::endl;
	std::cout << "物理内存 - 可用: " << status.availablePhysical / (1024 * 1024) << " MB" << std::endl;
	std::cout << "页面文件 - 总量: " << status.totalPageFile / (1024 * 1024) << " MB" << std::endl;
	std::cout << "页面文件 - 可用: " << status.availablePageFile / (1024 * 1024) << " MB" << std::endl;
	std::cout << "虚拟内存 - 总量: " << status.totalVirtual / (1024 * 1024) << " MB" << std::endl;
	std::cout << "虚拟内存 - 可用: " << status.availableVirtual / (1024 * 1024) << " MB" << std::endl;

	if (status.memoryLoad > 85) {
		std::cout << "警告: 系统内存使用率过高!" << std::endl;
	}
}

MemoryStatus GameMonitor::GetSystemMemoryStatus() {
	MemoryStatus status = { 0 };

	// 系统内存状态
	MEMORYSTATUSEX memStatus;
	memStatus.dwLength = sizeof(memStatus);
	if (GlobalMemoryStatusEx(&memStatus)) {
		status.totalPhysical = memStatus.ullTotalPhys;
		status.availablePhysical = memStatus.ullAvailPhys;
		status.totalPageFile = memStatus.ullTotalPageFile;
		status.availablePageFile = memStatus.ullAvailPageFile;
		status.totalVirtual = memStatus.ullTotalVirtual;
		status.availableVirtual = memStatus.ullAvailVirtual;
		status.memoryLoad = memStatus.dwMemoryLoad;
	}

	// 进程内存使用
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if (GetProcessMemoryInfo(GetCurrentProcess(),
		(PROCESS_MEMORY_COUNTERS*)&pmc,
		sizeof(pmc))) {
		status.processMemoryUsage = pmc.WorkingSetSize;
		status.peakProcessMemory = pmc.PeakWorkingSetSize;
	}

	return status;
}

void GameMonitor::PrintProcessInfo() {
	ProcessInfo info = GetProcessInfo();

	std::cout << "=== 进程信息 ===" << std::endl;
	std::cout << "进程ID: " << info.processId << std::endl;
	std::cout << "线程数量: " << info.threadCount << std::endl;
	std::cout << "句柄数量: " << info.handleCount << std::endl;
	std::cout << "工作集大小: " << info.workingSetSize / 1024 << " KB" << std::endl;
	std::cout << "页面文件使用: " << info.pagefileUsage / 1024 << " KB" << std::endl;
	std::cout << "执行文件路径: " << info.executablePath << std::endl;
}

ProcessInfo GameMonitor::GetProcessInfo() {
	ProcessInfo info = { 0 };
	info.processId = GetCurrentProcessId();

	// 获取线程数量
	info.threadCount = GetThreadCount();

	// 获取句柄数量
	info.handleCount = 0;
	HANDLE hProcess = GetCurrentProcess();
	if (GetProcessHandleCount(hProcess, &info.handleCount) == FALSE) {
		info.handleCount = 0;
	}

	// 获取内存信息
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
		info.workingSetSize = pmc.WorkingSetSize;
		info.pagefileUsage = pmc.PagefileUsage;
	}

	// 获取执行文件路径
	char exePath[MAX_PATH];
	if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
		info.executablePath = exePath;
	}

	return info;
}

void GameMonitor::UpdateFrame() {
	ULONGLONG currentTime = GetTickCount64();
	frameCount++;

	// 每秒更新一次性能指标
	if (currentTime - performance.lastUpdateTime >= 1000) {
		ULONGLONG elapsed = currentTime - performance.lastUpdateTime;
		performance.frameRate = (frameCount - performance.totalFrames) * 1000.0 / elapsed;
		performance.frameTime = 1000.0 / performance.frameRate;
		performance.totalFrames = frameCount;
		performance.lastUpdateTime = currentTime;
	}
}

void GameMonitor::PrintPerformanceMetrics() {
	std::cout << "=== 性能指标 ===" << std::endl;
	std::cout << "帧率: " << std::fixed << std::setprecision(2) << performance.frameRate << " FPS" << std::endl;
	std::cout << "帧时间: " << std::fixed << std::setprecision(2) << performance.frameTime << " ms" << std::endl;
	std::cout << "总帧数: " << performance.totalFrames << std::endl;

	if (performance.frameRate < 30.0) {
		std::cout << "  警告: 帧率过低!" << std::endl;
	}
}

PerformanceMetrics GameMonitor::GetPerformanceMetrics() {
	return performance;
}

DWORD GameMonitor::GetThreadCount() {
	DWORD threadCount = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

	if (hSnapshot != INVALID_HANDLE_VALUE) {
		THREADENTRY32 te32;
		te32.dwSize = sizeof(THREADENTRY32);

		DWORD currentProcessId = GetCurrentProcessId();

		if (Thread32First(hSnapshot, &te32)) {
			do {
				if (te32.th32OwnerProcessID == currentProcessId) {
					threadCount++;
				}
			} while (Thread32Next(hSnapshot, &te32));
		}

		CloseHandle(hSnapshot);
	}

	return threadCount;
}

void GameMonitor::PrintThreadInfo() {
	DWORD threadCount = GetThreadCount();
	std::cout << "=== 线程信息 ===" << std::endl;
	std::cout << "线程总数: " << threadCount << std::endl;
}

void GameMonitor::PrintLoadedModules() {
	std::cout << "=== 已加载模块 ===" << std::endl;

	HMODULE hModules[1024];
	DWORD cbNeeded;

	if (EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &cbNeeded)) {
		for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
			char szModName[MAX_PATH];
			if (GetModuleFileNameExA(GetCurrentProcess(), hModules[i], szModName, sizeof(szModName))) {
				std::cout << "模块 " << i << ": " << szModName << std::endl;
			}
		}
	}
}

void GameMonitor::CheckResourceLeaks() {
	std::cout << "=== 资源泄漏检查 ===" << std::endl;

	// 检查GDI对象泄漏
	int gdiObjects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	int userObjects = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);

	std::cout << "GDI对象: " << gdiObjects << std::endl;
	std::cout << "用户对象: " << userObjects << std::endl;

	if (gdiObjects > 1000) {
		std::cout << "警告: GDI对象数量过多，可能存在泄漏!" << std::endl;
	}

	if (userObjects > 1000) {
		std::cout << "警告: 用户对象数量过多，可能存在泄漏!" << std::endl;
	}
}

void GameMonitor::CheckSystemHealth() {
	std::cout << "=== 系统健康检查 ===" << std::endl;

	StackUsageInfo stackInfo = GetStackUsage();
	MemoryStatus memStatus = GetSystemMemoryStatus();

	bool isHealthy = true;

	if (stackInfo.usagePercent > 80.0) {
		std::cout << "栈使用率过高: " << stackInfo.usagePercent << "%" << std::endl;
		isHealthy = false;
	}

	if (memStatus.memoryLoad > 85) {
		std::cout << "系统内存负载过高: " << memStatus.memoryLoad << "%" << std::endl;
		isHealthy = false;
	}

	if (performance.frameRate < 30.0 && performance.totalFrames > 100) {
		std::cout << "帧率过低: " << performance.frameRate << " FPS" << std::endl;
		isHealthy = false;
	}

	SIZE_T privateBytes = GetPrivateBytes();
	if (privateBytes > 500 * 1024 * 1024) { // 500MB
		std::cout << "进程内存使用过高: " << privateBytes / (1024 * 1024) << " MB" << std::endl;
		isHealthy = false;
	}

	if (isHealthy) {
		std::cout << "系统状态健康" << std::endl;
	}
	else {
		std::cout << "发现系统健康问题，请检查!" << std::endl;
	}
}

void GameMonitor::PrintComprehensiveReport() {
	std::cout << "\n" << std::string(60, '=') << std::endl;
	std::cout << "游戏监控综合报告" << std::endl;
	std::cout << std::string(60, '=') << std::endl;

	PrintStackUsage();
	std::cout << std::endl;

	PrintHeapUsage();
	std::cout << std::endl;

	PrintSystemMemoryStatus();
	std::cout << std::endl;

	PrintProcessInfo();
	std::cout << std::endl;

	PrintPerformanceMetrics();
	std::cout << std::endl;

	PrintThreadInfo();
	std::cout << std::endl;

	CheckResourceLeaks();
	std::cout << std::endl;

	CheckSystemHealth();
	std::cout << std::string(60, '=') << std::endl;
}
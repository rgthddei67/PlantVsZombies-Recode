#include "GameMonitor.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <psapi.h>
#include <tlhelp32.h>

#pragma comment(lib, "psapi.lib")

// ---- 静态成员初始化 ----
PerformanceMetrics             GameMonitor::performance = {};
std::atomic<ULONGLONG>         GameMonitor::frameCount = 0;
ULONGLONG                      GameMonitor::lastFrameTime = 0;
HealthThresholds               GameMonitor::thresholds = {};
std::array<double, kFrameWindowSize> GameMonitor::frameTimeWindow = {};
int                            GameMonitor::frameWindowIndex = 0;
bool                           GameMonitor::frameWindowFull = false;
SRWLOCK                        GameMonitor::perfLock = SRWLOCK_INIT;
ULONGLONG                      GameMonitor::lastCpuKernel = 0;
ULONGLONG                      GameMonitor::lastCpuUser = 0;
ULONGLONG                      GameMonitor::lastCpuSystem = 0;

// ---- Init ----
void GameMonitor::Init() {
	// 预热 CPU 基准时间
	FILETIME ct, et, kt, ut, ist, iat;
	GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut);
	GetSystemTimes(&ist, &iat, nullptr);
	lastCpuKernel = (static_cast<ULONGLONG>(kt.dwHighDateTime) << 32) | kt.dwLowDateTime;
	lastCpuUser = (static_cast<ULONGLONG>(ut.dwHighDateTime) << 32) | ut.dwLowDateTime;
	lastCpuSystem = (static_cast<ULONGLONG>(iat.dwHighDateTime) << 32) | iat.dwLowDateTime;
}

void GameMonitor::SetHealthThresholds(const HealthThresholds& t) {
	thresholds = t;
}

// ---- 栈 ----
void GameMonitor::PrintStackUsage() {
	StackUsageInfo usage = GetStackUsage();
	std::cout << "=== 栈使用情况 ===" << std::endl;
	std::cout << "总保留大小: " << usage.totalReserved / 1024 << " KB" << std::endl;
	std::cout << "已使用: " << usage.used / 1024 << " KB" << std::endl;
	std::cout << "剩余: " << usage.remaining / 1024 << " KB" << std::endl;
	std::cout << "使用率: " << std::fixed << std::setprecision(2) << usage.usagePercent << "%" << std::endl;
	if (usage.usagePercent > thresholds.stackUsagePercent)
		std::cout << "  警告: 栈使用率较高!" << std::endl;
}

StackUsageInfo GameMonitor::GetStackUsage() {
	StackUsageInfo info = {};
	ULONG_PTR stackLow, stackHigh;
	ULONG_PTR currentStack = (ULONG_PTR)_AddressOfReturnAddress();
	GetCurrentThreadStackLimits(&stackLow, &stackHigh);
	info.totalReserved = stackHigh - stackLow;
	info.remaining = currentStack - stackLow;
	info.used = info.totalReserved - info.remaining;
	info.usagePercent = info.totalReserved > 0
		? static_cast<double>(info.used) / info.totalReserved * 100.0 : 0.0;
	info.stackLow = stackLow;
	info.stackHigh = stackHigh;
	return info;
}

// ---- 堆（与原版相同，略作整理）----
void GameMonitor::PrintHeapUsage() {
	SIZE_T heapUsage = GetProcessHeapUsage();
	SIZE_T privateBytes = GetPrivateBytes();
	std::cout << "=== 堆内存使用情况 ===" << std::endl;
	std::cout << "进程堆使用: " << heapUsage / 1024 << " KB" << std::endl;
	std::cout << "私有字节数: " << privateBytes / 1024 << " KB" << std::endl;

	HANDLE defaultHeap = GetProcessHeap();
	if (defaultHeap) {
		HEAP_SUMMARY hs = {};
		hs.cb = sizeof(hs);
		if (HeapSummary(defaultHeap, 0, &hs)) {
			std::cout << "默认堆 - 提交大小: " << hs.cbCommitted / 1024 << " KB" << std::endl;
			std::cout << "默认堆 - 保留大小: " << hs.cbReserved / 1024 << " KB" << std::endl;
		}
	}
}

SIZE_T GameMonitor::GetProcessHeapUsage() {
	SIZE_T total = 0;
	DWORD n = GetProcessHeaps(0, nullptr);
	if (n == 0) return 0;

	auto heaps = std::make_unique<HANDLE[]>(n);
	DWORD actual = GetProcessHeaps(n, heaps.get());
	for (DWORD i = 0; i < actual; i++) {
		HEAP_SUMMARY hs = {};
		hs.cb = sizeof(hs);
		if (HeapSummary(heaps[i], 0, &hs)) {
			total += hs.cbCommitted;
		}
	}
	return total;
}

SIZE_T GameMonitor::GetPrivateBytes() {
	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	if (GetProcessMemoryInfo(GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
		return pmc.PrivateUsage;
	return 0;
}

// ---- 系统内存 ----
void GameMonitor::PrintSystemMemoryStatus() {
	MemoryStatus s = GetSystemMemoryStatus();
	std::cout << "=== 系统内存状态 ===" << std::endl;
	std::cout << "内存负载: " << s.memoryLoad << "%" << std::endl;
	std::cout << "物理内存 - 总量: " << s.totalPhysical / (1024 * 1024) << " MB" << std::endl;
	std::cout << "物理内存 - 可用: " << s.availablePhysical / (1024 * 1024) << " MB" << std::endl;
	std::cout << "页面文件 - 总量: " << s.totalPageFile / (1024 * 1024) << " MB" << std::endl;
	std::cout << "页面文件 - 可用: " << s.availablePageFile / (1024 * 1024) << " MB" << std::endl;
	std::cout << "虚拟内存 - 总量: " << s.totalVirtual / (1024 * 1024) << " MB" << std::endl;
	std::cout << "虚拟内存 - 可用: " << s.availableVirtual / (1024 * 1024) << " MB" << std::endl;
	std::cout << "进程工作集: " << s.processMemoryUsage / (1024 * 1024) << " MB" << std::endl;
	std::cout << "进程峰值工作集: " << s.peakProcessMemory / (1024 * 1024) << " MB" << std::endl;
	if (s.memoryLoad > thresholds.memoryLoadPercent)
		std::cout << "警告: 系统内存使用率过高!" << std::endl;
}

MemoryStatus GameMonitor::GetSystemMemoryStatus() {
	MemoryStatus s = {};
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);
	if (GlobalMemoryStatusEx(&ms)) {
		s.totalPhysical = static_cast<SIZE_T>(ms.ullTotalPhys);
		s.availablePhysical = static_cast<SIZE_T>(ms.ullAvailPhys);
		s.totalPageFile = static_cast<SIZE_T>(ms.ullTotalPageFile);
		s.availablePageFile = static_cast<SIZE_T>(ms.ullAvailPageFile);
		s.totalVirtual = static_cast<SIZE_T>(ms.ullTotalVirtual);
		s.availableVirtual = static_cast<SIZE_T>(ms.ullAvailVirtual);
		s.memoryLoad = ms.dwMemoryLoad;
	}
	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	if (GetProcessMemoryInfo(GetCurrentProcess(),
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
		s.processMemoryUsage = pmc.WorkingSetSize;
		s.peakProcessMemory = pmc.PeakWorkingSetSize;
	}
	return s;
}

// ---- 进程信息 ----
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
	ProcessInfo info = {};
	info.processId = GetCurrentProcessId();
	info.threadCount = GetThreadCount();

	HANDLE hProcess = GetCurrentProcess();
	GetProcessHandleCount(hProcess, &info.handleCount);

	PROCESS_MEMORY_COUNTERS_EX pmc = {};
	if (GetProcessMemoryInfo(hProcess,
		reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
		info.workingSetSize = pmc.WorkingSetSize;
		info.pagefileUsage = pmc.PagefileUsage;
	}
	char exePath[MAX_PATH];
	if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
		info.executablePath = exePath;
	return info;
}

// ---- 性能（帧率 + 抖动，修复首帧 bug，加锁）----
void GameMonitor::UpdateFrame() {
	ULONGLONG currentTime = GetTickCount64();
	ULONGLONG fc = ++frameCount; // 原子递增

	// 首帧初始化：直接记录时间戳后返回，避免 elapsed 异常
	AcquireSRWLockExclusive(&perfLock);
	if (performance.lastUpdateTime == 0) {
		performance.lastUpdateTime = currentTime;
		lastFrameTime = currentTime;
		ReleaseSRWLockExclusive(&perfLock);
		return;
	}

	// 记录本帧帧时间到滑动窗口
	double ft = static_cast<double>(currentTime - lastFrameTime);
	lastFrameTime = currentTime;
	frameTimeWindow[frameWindowIndex] = ft;
	frameWindowIndex = (frameWindowIndex + 1) % kFrameWindowSize;
	if (!frameWindowFull && frameWindowIndex == 0) frameWindowFull = true;

	// 每秒更新汇总指标
	if (currentTime - performance.lastUpdateTime >= 1000) {
		ULONGLONG elapsed = currentTime - performance.lastUpdateTime;
		ULONGLONG frames = fc - performance.totalFrames;

		performance.frameRate = frames * 1000.0 / elapsed;
		performance.frameTime = performance.frameRate > 0.0
			? 1000.0 / performance.frameRate : 0.0;
		performance.totalFrames = fc;
		performance.lastUpdateTime = currentTime;

		// 计算窗口内抖动统计
		int count = frameWindowFull ? kFrameWindowSize : frameWindowIndex;
		if (count > 0) {
			double minFt = frameTimeWindow[0], maxFt = frameTimeWindow[0], sum = 0.0;
			for (int i = 0; i < count; i++) {
				double v = frameTimeWindow[i];
				if (v < minFt) minFt = v;
				if (v > maxFt) maxFt = v;
				sum += v;
			}
			double mean = sum / count;
			double var = 0.0;
			for (int i = 0; i < count; i++) {
				double d = frameTimeWindow[i] - mean;
				var += d * d;
			}
			performance.frameTimeMin = minFt;
			performance.frameTimeMax = maxFt;
			performance.frameTimeStdDev = std::sqrt(var / count);
		}
	}
	ReleaseSRWLockExclusive(&perfLock);
}

void GameMonitor::PrintPerformanceMetrics() {
	AcquireSRWLockShared(&perfLock);
	PerformanceMetrics p = performance;
	ReleaseSRWLockShared(&perfLock);

	std::cout << "=== 性能指标 ===" << std::endl;
	std::cout << "帧率: " << std::fixed << std::setprecision(2) << p.frameRate << " FPS" << std::endl;
	std::cout << "平均帧时间: " << std::fixed << std::setprecision(2) << p.frameTime << " ms" << std::endl;
	std::cout << "最小帧时间: " << std::fixed << std::setprecision(2) << p.frameTimeMin << " ms" << std::endl;
	std::cout << "最大帧时间: " << std::fixed << std::setprecision(2) << p.frameTimeMax << " ms" << std::endl;
	std::cout << "帧时间标准差: " << std::fixed << std::setprecision(2) << p.frameTimeStdDev << " ms (抖动)" << std::endl;
	std::cout << "总帧数: " << p.totalFrames << std::endl;

	if (p.frameRate < thresholds.minFrameRate)
		std::cout << "  警告: 帧率过低!" << std::endl;
	if (p.frameTimeStdDev > 8.0)
		std::cout << "  警告: 帧时间抖动较大，可能存在卡顿!" << std::endl;
}

PerformanceMetrics GameMonitor::GetPerformanceMetrics() {
	AcquireSRWLockShared(&perfLock);
	PerformanceMetrics p = performance;
	ReleaseSRWLockShared(&perfLock);
	return p;
}

// ---- CPU 占用（新增）----
double GameMonitor::GetCpuUsage() {
	FILETIME ct, et, kt, ut;
	if (!GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut))
		return -1.0;

	FILETIME ist, iat, ibt;
	if (!GetSystemTimes(&ibt, &iat, &ist))
		return -1.0;

	auto toULL = [](const FILETIME& ft) -> ULONGLONG {
		return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
		};

	ULONGLONG procKernel = toULL(kt);
	ULONGLONG procUser = toULL(ut);
	ULONGLONG sysIdle = toULL(iat);

	ULONGLONG deltaKernel = procKernel - lastCpuKernel;
	ULONGLONG deltaUser = procUser - lastCpuUser;
	ULONGLONG deltaSystem = sysIdle - lastCpuSystem;

	lastCpuKernel = procKernel;
	lastCpuUser = procUser;
	lastCpuSystem = sysIdle;

	// deltaSystem 是所有核心的总时间片（100ns 单位）
	if (deltaSystem == 0) return 0.0;
	return static_cast<double>(deltaKernel + deltaUser) / deltaSystem * 100.0;
}

void GameMonitor::PrintCpuUsage() {
	double cpu = GetCpuUsage();
	std::cout << "=== CPU 占用 ===" << std::endl;
	if (cpu < 0.0) {
		std::cout << "获取 CPU 数据失败" << std::endl;
	}
	else {
		std::cout << "进程 CPU 占用: " << std::fixed << std::setprecision(2) << cpu << "%" << std::endl;
		if (cpu > thresholds.maxCpuUsagePercent)
			std::cout << "  警告: CPU 占用过高!" << std::endl;
	}
}

// ---- 线程 ----
DWORD GameMonitor::GetThreadCount() {
	DWORD count = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnap == INVALID_HANDLE_VALUE) return 0;
	THREADENTRY32 te = {};
	te.dwSize = sizeof(te);
	DWORD pid = GetCurrentProcessId();
	if (Thread32First(hSnap, &te)) {
		do { if (te.th32OwnerProcessID == pid) count++; } while (Thread32Next(hSnap, &te));
	}
	CloseHandle(hSnap);
	return count;
}

void GameMonitor::PrintThreadInfo() {
	std::cout << "=== 线程信息 ===" << std::endl;
	std::cout << "线程总数: " << GetThreadCount() << std::endl;
}

// ---- 模块（修复：动态分配）----
void GameMonitor::PrintLoadedModules() {
	std::cout << "=== 已加载模块 ===" << std::endl;
	HANDLE hProcess = GetCurrentProcess();
	DWORD cbNeeded = 0;

	// 第一次调用：获取所需字节数
	EnumProcessModules(hProcess, nullptr, 0, &cbNeeded);
	if (cbNeeded == 0) return;

	DWORD count = cbNeeded / sizeof(HMODULE);
	auto modules = std::make_unique<HMODULE[]>(count);

	if (!EnumProcessModules(hProcess, modules.get(), cbNeeded, &cbNeeded)) return;
	count = cbNeeded / sizeof(HMODULE); // 以实际返回为准

	for (DWORD i = 0; i < count; i++) {
		char name[MAX_PATH];
		if (GetModuleFileNameExA(hProcess, modules[i], name, MAX_PATH))
			std::cout << "模块 " << i << ": " << name << std::endl;
	}
}

// ---- 资源泄漏检测 ----
void GameMonitor::CheckResourceLeaks() {
	std::cout << "=== 资源泄漏检查 ===" << std::endl;
	int gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	int user = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
	std::cout << "GDI 对象: " << gdi << std::endl;
	std::cout << "用户对象: " << user << std::endl;
	if (gdi > 1000) std::cout << "警告: GDI 对象数量过多，可能存在泄漏!" << std::endl;
	if (user > 1000) std::cout << "警告: 用户对象数量过多，可能存在泄漏!" << std::endl;
}

// ---- 健康检查（接入 CPU 告警）----
void GameMonitor::CheckSystemHealth() {
	std::cout << "=== 系统健康检查 ===" << std::endl;
	bool ok = true;

	StackUsageInfo si = GetStackUsage();
	if (si.usagePercent > thresholds.stackUsagePercent) {
		std::cout << "栈使用率过高: " << si.usagePercent << "%" << std::endl;
		ok = false;
	}

	MemoryStatus ms = GetSystemMemoryStatus();
	if (ms.memoryLoad > thresholds.memoryLoadPercent) {
		std::cout << "系统内存负载过高: " << ms.memoryLoad << "%" << std::endl;
		ok = false;
	}

	PerformanceMetrics p = GetPerformanceMetrics();
	if (p.frameRate < thresholds.minFrameRate && p.totalFrames > 100) {
		std::cout << "帧率过低: " << p.frameRate << " FPS" << std::endl;
		ok = false;
	}

	SIZE_T priv = GetPrivateBytes();
	if (priv > thresholds.maxPrivateBytes) {
		std::cout << "进程内存使用过高: " << priv / (1024 * 1024) << " MB" << std::endl;
		ok = false;
	}

	double cpu = GetCpuUsage();
	if (cpu > thresholds.maxCpuUsagePercent) {
		std::cout << "CPU 占用过高: " << cpu << "%" << std::endl;
		ok = false;
	}

	std::cout << (ok ? "系统状态健康" : "发现系统健康问题，请检查!") << std::endl;
}

// ---- 综合报告 ----
void GameMonitor::PrintComprehensiveReport() {
	std::cout << "\n" << std::string(60, '=') << std::endl;
	std::cout << "游戏监控综合报告" << std::endl;
	std::cout << std::string(60, '=') << std::endl;

	PrintStackUsage();            std::cout << std::endl;
	PrintHeapUsage();             std::cout << std::endl;
	PrintSystemMemoryStatus();    std::cout << std::endl;
	PrintProcessInfo();           std::cout << std::endl;
	PrintPerformanceMetrics();    std::cout << std::endl;
	PrintCpuUsage();              std::cout << std::endl;
	PrintThreadInfo();            std::cout << std::endl;
	CheckResourceLeaks();         std::cout << std::endl;
	CheckSystemHealth();
	std::cout << std::string(60, '=') << std::endl;
}
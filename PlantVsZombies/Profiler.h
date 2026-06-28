#pragma once
#ifndef _PROFILER_H
#define _PROFILER_H

// ============================================================
//  性能埋点 —— 用于定位掉帧的瓶颈阶段。
//  默认完全休眠：只有在启动参数里加 -Profile 时才会累加并打印 FRAME PROFILE。
//  开关由 g_ProfileEnabled（定义在 Profiler.cpp，main.cpp 解析 -Profile 置位）控制。
//  注意：所有 PROFILE_SCOPE / CountFlush 只能在主线程调用（非线程安全）。
// ============================================================

#include <chrono>
#include <map>
#include <string>
#include <cstdio>

// 全局开关：false 时 Profiler 的所有累加/打印入口立即返回（零开销）。
// 唯一定义在 Profiler.cpp，避免头文件被多个翻译单元 include 时的 ODR 重定义。
extern bool g_ProfileEnabled;

class Profiler {
public:
	static Profiler& Get() {
		static Profiler instance;
		return instance;
	}

	using Clock = std::chrono::steady_clock;

	void Add(const std::string& name, double ms) {
		if (!g_ProfileEnabled) return;
		mAccum[name] += ms;
	}

	// 在 Graphics::FlushBatch 真正提交时调用，verts = 本次刷新的顶点数
	void CountFlush(size_t verts) {
		if (!g_ProfileEnabled) return;
		mFlushCount++;
		mFlushVerts += verts;
	}

	// 诊断：每次 GetOrCreateTextTexture 调用记一次（miss=true 表示走了 TTF 光栅化+GPU 上传）。
	// 用于把 7.Draw_replay 的串行成本拆成「整串→纹理缓存 thrash」与「逐行 draw call 地板」。
	void CountText(bool miss) {
		if (!g_ProfileEnabled) return;
		mTextTotalAccum++;
		if (miss) mTextMissAccum++;
	}

	// 每帧调用一次（主循环末尾）。每 kReportFrames 帧打印一次平均值。
	void EndFrame() {
		if (!g_ProfileEnabled) return;
		auto now = Clock::now();
		if (mHasLastFrame) {
			double frameMs = std::chrono::duration<double, std::milli>(now - mLastFrame).count();
			mFrameAccum += frameMs;
		}
		mLastFrame = now;
		mHasLastFrame = true;

		mFlushCountAccum += mFlushCount;
		mFlushVertsAccum += mFlushVerts;
		mFlushCount = 0;
		mFlushVerts = 0;

		if (++mFrames < kReportFrames) return;

		double inv = 1.0 / static_cast<double>(mFrames);
		double avgFrame = mFrameAccum * inv;
		std::printf("\n==== FRAME PROFILE (avg over %d frames) ====\n", mFrames);
		std::printf("  total / frame        : %7.2f ms  (%.1f FPS)\n",
			avgFrame, avgFrame > 0.0 ? 1000.0 / avgFrame : 0.0);
		for (auto& kv : mAccum) {
			std::printf("  %-20s : %7.2f ms\n", kv.first.c_str(), kv.second * inv);
		}
		// 诊断计数（每帧均值）：textRaster(miss) 高 → 缓存被击穿；flushBatch 高 → 逐行 draw call 地板。
		std::printf("  %-20s : %7.1f /frame\n", "textDraw(lines)", mTextTotalAccum * inv);
		std::printf("  %-20s : %7.1f /frame\n", "textRaster(miss)", mTextMissAccum * inv);
		std::printf("  %-20s : %7.1f /frame\n", "flushBatch", static_cast<double>(mFlushCountAccum) * inv);
		std::printf("============================================\n");

		mAccum.clear();
		mFrameAccum = 0.0;
		mFlushCountAccum = 0;
		mFlushVertsAccum = 0;
		mTextMissAccum = 0;
		mTextTotalAccum = 0;
		mFrames = 0;
	}

private:
	static constexpr int kReportFrames = 60;
	std::map<std::string, double> mAccum;
	double mFrameAccum = 0.0;
	Clock::time_point mLastFrame;
	bool mHasLastFrame = false;
	int mFrames = 0;
	size_t mFlushCount = 0;
	size_t mFlushVerts = 0;
	size_t mFlushCountAccum = 0;
	size_t mFlushVertsAccum = 0;
	size_t mTextMissAccum = 0;    // 诊断：窗口内文字缓存未命中(光栅化)总次数
	size_t mTextTotalAccum = 0;   // 诊断：窗口内文字绘制(行)总次数
};

// RAII 计时：作用域结束时把耗时累加到对应名字
class ScopedProfile {
public:
	explicit ScopedProfile(const char* name)
		: mName(name) {
		if (!g_ProfileEnabled) return;   // 禁用时连时钟都不读
		mStart = Profiler::Clock::now();
	}
	~ScopedProfile() {
		if (!g_ProfileEnabled) return;   // 禁用时零测量、零 std::string 临时构造（真正零开销）
		double ms = std::chrono::duration<double, std::milli>(
			Profiler::Clock::now() - mStart).count();
		Profiler::Get().Add(mName, ms);
	}
private:
	const char* mName;
	Profiler::Clock::time_point mStart{};
};

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE(name) ScopedProfile PROFILE_CONCAT(_prof_, __LINE__)(name)

#endif

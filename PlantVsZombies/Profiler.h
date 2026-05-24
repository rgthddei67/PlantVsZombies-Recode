#pragma once
#ifndef _PROFILER_H
#define _PROFILER_H

// ============================================================
//  临时性能埋点 —— 用于定位 4400 僵尸掉帧的瓶颈阶段。
//  确认瓶颈后可整体删除本文件及其 #include / PROFILE_SCOPE 调用。
//  注意：所有 PROFILE_SCOPE / CountFlush 只能在主线程调用（非线程安全）。
// ============================================================

#include <chrono>
#include <map>
#include <string>
#include <cstdio>

class Profiler {
public:
	static Profiler& Get() {
		static Profiler instance;
		return instance;
	}

	using Clock = std::chrono::steady_clock;

	void Add(const std::string& name, double ms) {
		mAccum[name] += ms;
	}

	// 在 Graphics::FlushBatch 真正提交时调用，verts = 本次刷新的顶点数
	void CountFlush(size_t verts) {
		mFlushCount++;
		mFlushVerts += verts;
	}

	// 每帧调用一次（主循环末尾）。每 kReportFrames 帧打印一次平均值。
	void EndFrame() {
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
		double avgFlush = mFlushCountAccum * inv;
		double avgSprites = (mFlushVertsAccum * inv) / 6.0;
		std::printf("  FlushBatch calls     : %7.1f /frame  (~%.0f sprites/frame)\n",
			avgFlush, avgSprites);
		std::printf("============================================\n");

		mAccum.clear();
		mFrameAccum = 0.0;
		mFlushCountAccum = 0;
		mFlushVertsAccum = 0;
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
};

// RAII 计时：作用域结束时把耗时累加到对应名字
class ScopedProfile {
public:
	explicit ScopedProfile(const char* name)
		: mName(name), mStart(Profiler::Clock::now()) {
	}
	~ScopedProfile() {
		double ms = std::chrono::duration<double, std::milli>(
			Profiler::Clock::now() - mStart).count();
		Profiler::Get().Add(mName, ms);
	}
private:
	const char* mName;
	Profiler::Clock::time_point mStart;
};

#define PROFILE_CONCAT_INNER(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_INNER(a, b)
#define PROFILE_SCOPE(name) ScopedProfile PROFILE_CONCAT(_prof_, __LINE__)(name)

#endif

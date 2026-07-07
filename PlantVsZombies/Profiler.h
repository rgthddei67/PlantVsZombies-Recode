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

	// 诊断：BuildGlyphAtlas 每次全量重建记一次。正常应为 0/frame（图集建好后永久命中）；
	// 持续 >0 = 重建循环（建失败 textureID 保持 0 → 每次 DrawGlyphRun 都重建，TTF 全字集
	// 光栅化 + 建/销毁 GPU 纹理，约 1ms/次，是 Draw_replay 串行尖峰的头号嫌疑）。
	void CountGlyphBuild() {
		if (!g_ProfileEnabled) return;
		mGlyphBuildAccum++;
	}

	// 诊断：DrawGlyphRun 整串回退 DrawText 记一次（buildFail=true 表示图集建失败回退，
	// false 表示图集健在但缺字形回退）。回退行会再被 textDraw(lines) 计数。
	void CountGlyphFallback(bool buildFail) {
		if (!g_ProfileEnabled) return;
		if (buildFail) mGlyphFbBuildAccum++; else mGlyphFbMissAccum++;
	}

	// 诊断：DrawGlyphRun 快路径每绘制一行记一次（textDraw(lines) 只计整串 DrawText，不含此路径）。
	// 血量显示的真实行数看这里——它决定 replay 里 inline flush/重绑的次数。
	void CountGlyphLine() {
		if (!g_ProfileEnabled) return;
		mGlyphLineAccum++;
	}

	// 诊断：碰撞 sweep-and-prune 每帧统计。iter=行内层扫描总迭代次数（O(k²) 退化项），
	// reject=被层掩码 CanCollide 拒绝（纯浪费的迭代），check=真正做了 AABB 检测，
	// hit=检出的碰撞对。由 CollisionSystem::Update 在并行派发结束后于主线程调用一次。
	void CountSweep(size_t iter, size_t reject, size_t check, size_t hit) {
		if (!g_ProfileEnabled) return;
		mSweepIterAccum += iter;
		mSweepRejectAccum += reject;
		mSweepCheckAccum += check;
		mSweepHitAccum += hit;
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
		// 字形图集诊断：glyphAtlasBuild>0 = 图集每帧重建循环；glyphFb(build)>0 = 建失败回退整串
		// DrawText；glyphFb(missing)>0 = 图集健在但缺字形回退。三者全 0 才说明血量走的是图集快路径。
		std::printf("  %-20s : %7.1f /frame\n", "glyphRun(lines)", static_cast<double>(mGlyphLineAccum) * inv);
		std::printf("  %-20s : %7.1f /frame\n", "glyphAtlasBuild", static_cast<double>(mGlyphBuildAccum) * inv);
		std::printf("  %-20s : %7.1f /frame\n", "glyphFb(build)", static_cast<double>(mGlyphFbBuildAccum) * inv);
		std::printf("  %-20s : %7.1f /frame\n", "glyphFb(missing)", static_cast<double>(mGlyphFbMissAccum) * inv);
		// 碰撞 sweep 诊断：iter 巨大且 reject≈iter → SAP 在密集同行退化成 O(k²)，且几乎全是被层掩码
		// 拒绝的僵尸×僵尸空转；check/hit 才是真正有用的工作量。
		std::printf("  %-20s : %12.0f /frame\n", "sweepIter", static_cast<double>(mSweepIterAccum) * inv);
		std::printf("  %-20s : %12.0f /frame\n", "sweepReject", static_cast<double>(mSweepRejectAccum) * inv);
		std::printf("  %-20s : %12.0f /frame\n", "sweepCheck", static_cast<double>(mSweepCheckAccum) * inv);
		std::printf("  %-20s : %12.0f /frame\n", "sweepHit", static_cast<double>(mSweepHitAccum) * inv);
		std::printf("============================================\n");

		mAccum.clear();
		mFrameAccum = 0.0;
		mFlushCountAccum = 0;
		mFlushVertsAccum = 0;
		mTextMissAccum = 0;
		mTextTotalAccum = 0;
		mGlyphLineAccum = 0;
		mGlyphBuildAccum = 0;
		mGlyphFbBuildAccum = 0;
		mGlyphFbMissAccum = 0;
		mSweepIterAccum = 0;
		mSweepRejectAccum = 0;
		mSweepCheckAccum = 0;
		mSweepHitAccum = 0;
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
	size_t mGlyphLineAccum = 0;   // 诊断：窗口内 DrawGlyphRun 快路径绘制行数
	size_t mGlyphBuildAccum = 0;  // 诊断：窗口内 BuildGlyphAtlas 全量重建次数
	size_t mGlyphFbBuildAccum = 0;// 诊断：窗口内 DrawGlyphRun 因图集建失败回退 DrawText 次数
	size_t mGlyphFbMissAccum = 0; // 诊断：窗口内 DrawGlyphRun 因缺字形回退 DrawText 次数
	size_t mSweepIterAccum = 0;   // 诊断：窗口内碰撞 sweep 内层总迭代次数
	size_t mSweepRejectAccum = 0; // 诊断：窗口内被 CanCollide 拒绝的迭代次数
	size_t mSweepCheckAccum = 0;  // 诊断：窗口内真正做 AABB 检测的次数
	size_t mSweepHitAccum = 0;    // 诊断：窗口内检出的碰撞对数
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

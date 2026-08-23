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
		double& maximum = mMaxPerCall[name];
		if (ms > maximum) maximum = ms;
		++mCallCount[name];
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

	/**
	 * @brief 记录并行绘制 worker 的负载与有效对象数。
	 *
	 * 只能在 Dispatch 全部结束后由主线程调用；recordSlots 是保持顺序的逻辑切片数，
	 * workerElapsedSumMs 是各物理 worker 墙钟耗时之和，longestWorkerMs 是其中最长者。
	 * 两者用于区分录制本身昂贵与 worker 负载不均。
	 */
	void CountParallelDraw(size_t activeObjects, size_t workers, size_t recordSlots,
		double workerElapsedSumMs, double longestWorkerMs) {
		if (!g_ProfileEnabled) return;
		mDrawActiveObjectAccum += activeObjects;
		mDrawWorkerCountAccum += workers;
		mDrawRecordSlotCountAccum += recordSlots;
		mDrawWorkerElapsedSumMs += workerElapsedSumMs;
		mDrawLongestWorkerMs += longestWorkerMs;
		if (longestWorkerMs > mDrawLongestWorkerMaxMs) mDrawLongestWorkerMaxMs = longestWorkerMs;
		if (workers > 0 && longestWorkerMs > 0.0) {
			mDrawWorkerBalanceAccum += workerElapsedSumMs
				/ (static_cast<double>(workers) * longestWorkerMs);
		}
	}

	/**
	 * @brief 记录并行绘制实际生成的 GPU 数据与命令数量。
	 *
	 * 由 ReplayAndEndParallel 在主线程汇总各 worker slice 后调用；demand 包含因容量不足而
	 * 被拒绝的写入，因此即使溢出也能反映真实工作量。
	 */
	void CountParallelRecord(size_t batchVertices, size_t matrices,
		size_t instances, size_t commands) {
		if (!g_ProfileEnabled) return;
		mDrawBatchVertexAccum += batchVertices;
		mDrawMatrixAccum += matrices;
		mDrawInstanceAccum += instances;
		mDrawCommandAccum += commands;
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
			const size_t calls = mCallCount[kv.first];
			const double averagePerCall = calls > 0
				? kv.second / static_cast<double>(calls) : 0.0;
			std::printf("  %-30s : %7.2f ms/f | avg %7.2f | max %7.2f ms | %5zu calls\n",
				kv.first.c_str(), kv.second * inv, averagePerCall,
				mMaxPerCall[kv.first], calls);
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
		// 并行绘制诊断：longest 接近 6.Draw_submit 而 balance 高，说明 worker record 是主成本；
		// balance 低则优先处理切片负载不均。实例/矩阵/顶点数用于把耗时归一化到真实提交量。
		std::printf("  %-20s : %12.0f /frame\n", "drawActiveObjects", static_cast<double>(mDrawActiveObjectAccum) * inv);
		std::printf("  %-20s : %12.1f /frame\n", "drawWorkers", static_cast<double>(mDrawWorkerCountAccum) * inv);
		std::printf("  %-20s : %12.1f /frame\n", "drawRecordSlots", static_cast<double>(mDrawRecordSlotCountAccum) * inv);
		std::printf("  %-20s : %12.2f ms/frame\n", "drawWorkerElapsedSum", mDrawWorkerElapsedSumMs * inv);
		std::printf("  %-20s : %12.2f ms/frame | max %7.2f ms\n",
			"drawLongestWorker", mDrawLongestWorkerMs * inv, mDrawLongestWorkerMaxMs);
		std::printf("  %-20s : %11.1f %%\n", "drawWorkerBalance", mDrawWorkerBalanceAccum * inv * 100.0);
		std::printf("  %-20s : %12.0f /frame\n", "recordBatchVertices", static_cast<double>(mDrawBatchVertexAccum) * inv);
		std::printf("  %-20s : %12.0f /frame\n", "recordMatrices", static_cast<double>(mDrawMatrixAccum) * inv);
		std::printf("  %-20s : %12.0f /frame\n", "recordInstances", static_cast<double>(mDrawInstanceAccum) * inv);
		std::printf("  %-20s : %12.0f /frame\n", "recordCommands", static_cast<double>(mDrawCommandAccum) * inv);
		std::printf("============================================\n");

		mAccum.clear();
		mMaxPerCall.clear();
		mCallCount.clear();
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
		mDrawActiveObjectAccum = 0;
		mDrawWorkerCountAccum = 0;
		mDrawRecordSlotCountAccum = 0;
		mDrawWorkerElapsedSumMs = 0.0;
		mDrawLongestWorkerMs = 0.0;
		mDrawLongestWorkerMaxMs = 0.0;
		mDrawWorkerBalanceAccum = 0.0;
		mDrawBatchVertexAccum = 0;
		mDrawMatrixAccum = 0;
		mDrawInstanceAccum = 0;
		mDrawCommandAccum = 0;
		mFrames = 0;
	}

private:
	static constexpr int kReportFrames = 60;
	std::map<std::string, double> mAccum;
	std::map<std::string, double> mMaxPerCall; // 当前报告窗口内每个作用域的单次最大耗时
	std::map<std::string, size_t> mCallCount;  // 当前报告窗口内每个作用域的调用次数
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
	size_t mDrawActiveObjectAccum = 0; // 诊断：窗口内参与并行 Draw 的有效对象数
	size_t mDrawWorkerCountAccum = 0;  // 诊断：窗口内参与并行 Draw 的 worker 数
	size_t mDrawRecordSlotCountAccum = 0; // 诊断：窗口内按顺序回放的逻辑录制切片数
	double mDrawWorkerElapsedSumMs = 0.0; // 诊断：窗口内各 worker 墙钟耗时之和
	double mDrawLongestWorkerMs = 0.0;    // 诊断：窗口内每帧最长 worker 耗时之和
	double mDrawLongestWorkerMaxMs = 0.0; // 诊断：窗口内单帧最长 worker 的最大耗时
	double mDrawWorkerBalanceAccum = 0.0; // 诊断：sum/(workers*longest)，1 表示完全均衡
	size_t mDrawBatchVertexAccum = 0; // 诊断：窗口内 worker 想写入的 batch 顶点数
	size_t mDrawMatrixAccum = 0;      // 诊断：窗口内 worker 想写入的矩阵数
	size_t mDrawInstanceAccum = 0;    // 诊断：窗口内 worker 想写入的实例数
	size_t mDrawCommandAccum = 0;     // 诊断：窗口内 worker 录制的状态/延迟文字命令数
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

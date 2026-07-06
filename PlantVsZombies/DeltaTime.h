#pragma once
#ifndef _DELTATIME_H
#define _DELTATIME_H

#include <SDL2/SDL.h>

// 固定步长时钟（2026-07-06 由变步长迁移）。
// BeginFrame 只累积墙钟时间，折算出本渲染帧应执行的逻辑步数（封顶 kMaxCatchUpSteps，
// 超出的欠账直接丢弃——过载时表现为整体慢动作，而不是单步大位移/子弹穿隧）；
// 每个逻辑步由 BeginStep 装填 deltaTime = kFixedStep * timeScale，
// 因此全仓库所有 GetDeltaTime() 调用点无需感知这次迁移。
// 收益：同 -Seed 下逻辑帧序列与墙钟解耦，AutoTest 可精确复现。
class DeltaTime {
private:
	static constexpr float kFixedStep = 1.0f / 60.0f;
	static constexpr int kMaxCatchUpSteps = 3;

	inline static Uint64 lastTime = 0;
	// 尚未折算成逻辑步的墙钟余量（秒）
	inline static float accumulator = 0.0f;
	inline static float unscaledDeltaTime = kFixedStep;
	inline static float deltaTime = kFixedStep;
	inline static bool isPaused = false;
	inline static float timeScale = 1.0f;
	// 暂停前保存的时间缩放系数，恢复时还原（避免暂停后丢失用户选择的速度）
	inline static float savedTimeScale = 1.0f;

	// 游戏总时间统计
	inline static double totalGameTime = 0.0;
	inline static double unscaledTotalTime = 0.0;

	DeltaTime() = delete;
	DeltaTime(const DeltaTime&) = delete;
	DeltaTime& operator=(const DeltaTime&) = delete;

public:
	// 渲染帧开头调用一次：返回本帧应执行的逻辑步数（0..kMaxCatchUpSteps）。
	// 暂停不冻结步进——菜单/UI 仍需 Update 消费点击，只是每步 dt 为 0（见 BeginStep）。
	static int BeginFrame() {
		Uint64 currentTime = SDL_GetTicks64();
		if (lastTime > 0) {
			accumulator += (currentTime - lastTime) / 1000.0f;
		}
		lastTime = currentTime;

		int steps = static_cast<int>(accumulator / kFixedStep);
		if (steps > kMaxCatchUpSteps) {
			steps = kMaxCatchUpSteps;
			accumulator = 0.0f;   // 丢债：追不上的逻辑时间不再补
		}
		else {
			accumulator -= steps * kFixedStep;
		}
		return steps;
	}

	// 每个逻辑步开头调用：装填本步的 deltaTime / unscaledDeltaTime。
	static void BeginStep() {
		if (isPaused) {
			unscaledDeltaTime = 0.0f;
			deltaTime = 0.0f;
			return;
		}
		unscaledDeltaTime = kFixedStep;
		deltaTime = kFixedStep * timeScale;
		totalGameTime += static_cast<double>(deltaTime);
		unscaledTotalTime += static_cast<double>(unscaledDeltaTime);
	}

	// 获取缩放后的DeltaTime
	static float GetDeltaTime() { return deltaTime; }

	// 获取未缩放的DeltaTime（固定步长；暂停时为 0）
	static float GetUnscaledDeltaTime() { return unscaledDeltaTime; }

	// 逻辑步长常量（秒）
	static constexpr float GetFixedStep() { return kFixedStep; }

	// 设置时间缩放系数
	static void SetTimeScale(float scale) {
		timeScale = scale;
		isPaused = (scale == 0.0f);
	}

	// 获取当前时间缩放系数
	static float GetTimeScale() { return timeScale; }

	// 暂停/恢复游戏
	static void SetPaused(bool paused) {
		if (paused) {
			if (!isPaused) {
				savedTimeScale = timeScale;
				timeScale = 0.0f;
			}
		}
		else {
			if (isPaused) {
				timeScale = (savedTimeScale > 0.0f) ? savedTimeScale : 1.0f;
			}
		}
		isPaused = paused;
	}

	// 检查是否暂停
	static bool IsPaused() { return isPaused; }

	// 获取游戏总时间（缩放后）
	static double GetTotalTime() { return totalGameTime; }

	// 获取未缩放的游戏总时间
	static double GetUnscaledTotalTime() { return unscaledTotalTime; }

	// 重置
	static void Reset() {
		lastTime = 0;
		accumulator = 0.0f;
		unscaledDeltaTime = kFixedStep;
		deltaTime = kFixedStep;
		timeScale = 1.0f;
		savedTimeScale = 1.0f;
		isPaused = false;
		totalGameTime = 0.0;
		unscaledTotalTime = 0.0;
	}
};

#endif

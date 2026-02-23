#pragma once
#ifndef _DELTATIME_H
#define _DELTATIME_H

#include <SDL2/SDL.h>
#include <algorithm>

class DeltaTime {
private:
    inline static Uint64 lastTime = 0;
    inline static float unscaledDeltaTime = 0.016f;
    inline static float deltaTime = 0.016f;
    inline static float maxDeltaTime = 0.1f;
    inline static bool isPaused = false;
    inline static float timeScale = 1.0f;

    // 最小时间限制，防止调试时暂停导致的时间异常
    inline static float minDeltaTime = 0.0001f;

    // 游戏总时间统计
    inline static double totalGameTime = 0.0;
    inline static double unscaledTotalTime = 0.0;

    DeltaTime() = delete;
    DeltaTime(const DeltaTime&) = delete;
    DeltaTime& operator=(const DeltaTime&) = delete;

public:
    // 开始新的一帧，计算时间增量
    static void BeginFrame() {
        if (isPaused) {
            unscaledDeltaTime = 0.0f;
            deltaTime = 0.0f;
            // 更新 lastTime 避免恢复时产生巨大时间差
            lastTime = SDL_GetTicks64();
            return;
        }

        Uint64 currentTime = SDL_GetTicks64();
        if (lastTime > 0) {
            // 计算时间差（毫秒转换为秒）
            unscaledDeltaTime = (currentTime - lastTime) / 1000.0f;

            // 限制时间范围
            unscaledDeltaTime = std::clamp(unscaledDeltaTime, minDeltaTime, maxDeltaTime);

            // 应用时间缩放
            deltaTime = unscaledDeltaTime * timeScale;

            // 更新游戏总时间
            totalGameTime += static_cast<double>(deltaTime);
            unscaledTotalTime += static_cast<double>(unscaledDeltaTime);
        }
        lastTime = currentTime;
    }

    // 获取缩放后的DeltaTime
    static float GetDeltaTime() { return deltaTime; }

    // 获取未缩放的DeltaTime（原始帧时间）
    static float GetUnscaledDeltaTime() { return unscaledDeltaTime; }

    // 设置时间缩放系数
    static void SetTimeScale(float scale) {
        timeScale = scale;
        isPaused = (scale == 0.0f);
    }

    // 获取当前时间缩放系数
    static float GetTimeScale() { return timeScale; }

    // 暂停/恢复游戏
    static void SetPaused(bool paused) {
        isPaused = paused;
        if (paused) {
            timeScale = 0.0f;
        }
        else if (timeScale == 0.0f) {
            timeScale = 1.0f;  // 恢复时设为正常速度
        }
    }

    // 检查是否暂停
    static bool IsPaused() { return isPaused; }

    // 设置最大时间增量
    static void SetMaxDeltaTime(float max) {
        // 确保最大值有效
        if (max < minDeltaTime) max = minDeltaTime;
        if (max > 1.0f) max = 1.0f;
        maxDeltaTime = max;
    }

    // 获取最大时间增量
    static float GetMaxDeltaTime() { return maxDeltaTime; }

    // 设置最小时间增量
    static void SetMinDeltaTime(float min) {
        if (min < 0.000001f) min = 0.000001f;
        if (min > maxDeltaTime) min = maxDeltaTime;
        minDeltaTime = min;
    }

    // 获取最小时间增量
    static float GetMinDeltaTime() { return minDeltaTime; }

    // 获取游戏总时间（缩放后）
    static double GetTotalTime() { return totalGameTime; }

    // 获取未缩放的游戏总时间
    static double GetUnscaledTotalTime() { return unscaledTotalTime; }

    // 重置
    static void Reset() {
        lastTime = 0;
        unscaledDeltaTime = 0.016f;
        deltaTime = 0.016f;
        timeScale = 1.0f;
        isPaused = false;
        totalGameTime = 0.0;
        unscaledTotalTime = 0.0;
    }
};

#endif
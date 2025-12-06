#pragma once
#ifndef _DELTATIME_H
#define _DELTATIME_H
#include <SDL2/SDL.h>

class DeltaTime {
private:
    inline static Uint32 lastTime = 0;
    inline static float unscaledDeltaTime = 0.016f;  // 未缩放的时间增量
    inline static float deltaTime = 0.016f;          // 缩放后的时间增量
    inline static float maxDeltaTime = 0.1f;
    inline static bool isPaused = false;
    inline static float timeScale = 1.0f;           // 时间缩放系数

    DeltaTime() = delete;
    DeltaTime(const DeltaTime&) = delete;
    DeltaTime& operator=(const DeltaTime&) = delete;

public:
    // 开始新的一帧，计算时间增量
    static void BeginFrame() {
        if (isPaused) {
            unscaledDeltaTime = 0.0f;
            deltaTime = 0.0f;
            return;
        }

        Uint32 currentTime = SDL_GetTicks();
        if (lastTime > 0) {
            unscaledDeltaTime = (currentTime - lastTime) / 1000.0f;
            if (unscaledDeltaTime > maxDeltaTime) {
                unscaledDeltaTime = maxDeltaTime;
            }
            deltaTime = unscaledDeltaTime * timeScale;
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
    static void SetMaxDeltaTime(float max) { maxDeltaTime = max; }

    // 获取最大时间增量
    static float GetMaxDeltaTime() { return maxDeltaTime; }

    // 重置时间系统
    static void Reset() {
        lastTime = 0;
        unscaledDeltaTime = 0.016f;
        deltaTime = 0.016f;
        timeScale = 1.0f;
        isPaused = false;
    }
};

#endif
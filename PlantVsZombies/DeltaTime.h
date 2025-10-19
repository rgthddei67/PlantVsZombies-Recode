#pragma once
#ifndef _DELTATIME_H
#define _DELTATIME_H
#include <SDL2/SDL.h>

class DeltaTime {
private:
    Uint32 lastTime = 0;
    float deltaTime = 0.016f;
    float maxDeltaTime = 0.1f;
    bool isPaused = false;

public:
    static DeltaTime& GetInstance() {
        static DeltaTime instance;
        return instance;
    }

    void BeginFrame() {
        if (isPaused) {
            deltaTime = 0.0f;
            return;
        }

        Uint32 currentTime = SDL_GetTicks();
        if (lastTime > 0) {
            deltaTime = (currentTime - lastTime) / 1000.0f;
            if (deltaTime > maxDeltaTime) {
                deltaTime = maxDeltaTime;
            }
        }
        lastTime = currentTime;
    }

    static float GetDeltaTime() {
        return GetInstance().deltaTime;
    }

    void SetPaused(bool paused) {
        isPaused = paused;
    }

    void SetMaxDeltaTime(float max) {
        maxDeltaTime = max;
    }

    void Reset() {
        lastTime = 0;
        deltaTime = 0.016f;
    }

private:
    DeltaTime() = default;
    DeltaTime(const DeltaTime&) = delete;
    DeltaTime& operator=(const DeltaTime&) = delete;
};

#endif
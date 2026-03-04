#pragma once
#ifndef __PARTICLE_H__
#define __PARTICLE_H__

#include "../Graphics.h"
#include "../Game/Definit.h"
#include <memory>
#include <vector>

struct Particle {
    Vector position;
    Vector velocity;
    glm::vec4 color;
    glm::vec4 startColor;
    glm::vec4 endColor;
    float lifetime;
    float maxLifetime;
    float size;
    float startSize;
    float endSize;
    float rotation;
    float rotationSpeed;
    float gravity;
    bool active;
    bool fadeOut;
    const GLTexture* texture;

    // XML特性支持
    float brightness;           // 亮度
    float stretch;              // 拉伸
    glm::vec3 colorMultiplier;  // RGB颜色乘数
    int currentFrame;           // 当前动画帧
    float animationTimer;       // 动画计时器
    int totalFrames;            // 总帧数
    float frameRate;            // 帧率
    Vector fieldOffset;         // 场效果累加偏移
    Vector shakeOffset;         // 抖动偏移

    Particle();
    void Reset();
    void Update();
    void UpdateAnimation();     // 更新动画帧
};

#endif
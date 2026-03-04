#include "Particle.h"
#include "../DeltaTime.h"
#include <cmath>

Particle::Particle() : active(false) {
    Reset();
}

void Particle::Reset() {
    position = { 0, 0 };
    velocity = { 0, 0 };
    color = glm::vec4(255.0f);
    startColor = glm::vec4(255.0f);
    endColor = glm::vec4(255.0f, 255.0f, 255.0f, 0.0f);
    lifetime = 0.0f;
    maxLifetime = 60.0f;
    size = 1.0f;
    startSize = 1.0f;
    endSize = 0.5f;
    rotation = 0.0f;
    rotationSpeed = 0.0f;
    gravity = 0.0f;
    active = false;
    fadeOut = true;
    texture = nullptr;

    // XML特性初始化
    brightness = 1.0f;
    stretch = 1.0f;
    colorMultiplier = glm::vec3(1.0f);
    currentFrame = 0;
    animationTimer = 0.0f;
    totalFrames = 1;
    frameRate = 12.0f;
    fieldOffset = { 0, 0 };
    shakeOffset = { 0, 0 };
}

void Particle::Update() {
    if (!active) return;

    float deltaTime = DeltaTime::GetDeltaTime();
    lifetime += deltaTime;

    if (lifetime >= maxLifetime) {
        active = false;
        return;
    }

    velocity.y += gravity * deltaTime;
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;
    rotation += rotationSpeed * deltaTime;

    // 注意：size由ParticleEmitter通过XML配置的插值轨迹控制
    // 不再使用startSize和endSize的线性插值

    // 更新动画
    UpdateAnimation();
}

void Particle::UpdateAnimation() {
    if (totalFrames <= 1 || frameRate <= 0.0f) {
        return;
    }

    float deltaTime = DeltaTime::GetDeltaTime();
    animationTimer += deltaTime;

    float frameDuration = 1.0f / frameRate;
    if (animationTimer >= frameDuration) {
        animationTimer -= frameDuration;
        currentFrame = (currentFrame + 1) % totalFrames;
    }
}
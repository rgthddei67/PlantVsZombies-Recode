#include "Particle.h"
#include <cmath>

Particle::Particle() : active(false) {
    Reset();
}

void Particle::Reset() {
    position = { 0, 0 };
    velocity = { 0, 0 };
    color = { 255, 255, 255, 255 };
    startColor = { 255, 255, 255, 255 };
    endColor = { 255, 255, 255, 0 };
    lifetime = 0;
    maxLifetime = 60;
    size = 1.0f;
    startSize = 1.0f;
    endSize = 0.5f;
    rotation = 0.0f;
    rotationSpeed = 0.0f;
    gravity = 0.0f;
    active = false;
    fadeOut = true;
    texture = nullptr;
    textureRect = { 0, 0, 0, 0 };
    useTexture = false;
}

void Particle::Update() {
    if (!active) return;

    lifetime++;

    // 生命周期结束
    if (lifetime >= maxLifetime) {
        active = false;
        return;
    }

    // 应用物理
    velocity.y += gravity;
    position.x += velocity.x;
    position.y += velocity.y;
    rotation += rotationSpeed;

    // 计算插值
    float t = static_cast<float>(lifetime) / maxLifetime;

    // 大小插值
    size = startSize + (endSize - startSize) * t;

    // 颜色插值
    if (fadeOut) {
        color.r = static_cast<Uint8>(startColor.r + (endColor.r - startColor.r) * t);
        color.g = static_cast<Uint8>(startColor.g + (endColor.g - startColor.g) * t);
        color.b = static_cast<Uint8>(startColor.b + (endColor.b - startColor.b) * t);
        color.a = static_cast<Uint8>(startColor.a + (endColor.a - startColor.a) * t);
    }
}
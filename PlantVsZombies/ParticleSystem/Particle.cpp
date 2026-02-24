#include "Particle.h"
#include "../DeltaTime.h"
#include <cmath>

Particle::Particle() : active(false) {
    Reset();
}

void Particle::Reset() {
    position = { 0, 0 };
    velocity = { 0, 0 };
    color = glm::vec4(1.0f);
    startColor = glm::vec4(1.0f);
    endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
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
}

void Particle::Update() {
    if (!active) return;

    float deltaTime = DeltaTime::GetDeltaTime();
    lifetime += deltaTime;

    if (lifetime >= maxLifetime) {
        active = false;
        return;
    }

    // 物理更新
    velocity.y += gravity * deltaTime;
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;
    rotation += rotationSpeed * deltaTime;

    float t = lifetime / maxLifetime;
    size = startSize + (endSize - startSize) * t;

    // 颜色插值（fadeOut 控制透明度渐变）
    if (fadeOut) {
        color.r = startColor.r + (endColor.r - startColor.r) * t;
        color.g = startColor.g + (endColor.g - startColor.g) * t;
        color.b = startColor.b + (endColor.b - startColor.b) * t;
        color.a = startColor.a + (endColor.a - startColor.a) * t;
    }
}
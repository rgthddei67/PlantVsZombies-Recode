#pragma once
#ifndef __PARTICLE_H__
#define __PARTICLE_H__

#include "../Graphics.h"
#include "../Game/Definit.h"
#include <memory>
#include <vector>

enum class ParticleType {
    PEA_BULLET_HIT,
    ZOMBIE_HEAD_OFF,
    ZOMBIE_ARM_OFF,
    NUM_EFFECTS
};

// 基础粒子数据
struct Particle {
    Vector position;
    Vector velocity;
    glm::vec4 color;          // 当前颜色 (0~1)
    glm::vec4 startColor;     // 起始颜色 (0~1)
    glm::vec4 endColor;       // 结束颜色 (0~1)
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
    const GLTexture* texture; // 粒子纹理

    Particle();
    void Reset();
    void Update();
};

#endif
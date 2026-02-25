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

// 鍩虹绮掑瓙鏁版嵁
struct Particle {
    Vector position;
    Vector velocity;
    glm::vec4 color;          // 褰撳墠棰滆壊 (0~1)
    glm::vec4 startColor;     // 璧峰棰滆壊 (0~1)
    glm::vec4 endColor;       // 缁撴潫棰滆壊 (0~1)
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
    const GLTexture* texture; // 绮掑瓙绾圭悊

    Particle();
    void Reset();
    void Update();
};

#endif
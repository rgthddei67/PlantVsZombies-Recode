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
    float lifetime;
    float maxLifetime;
    float size;
    float rotation;
    float rotationSpeed;
    float gravity;
    bool active;
    const GLTexture* texture;

    float brightness;
    float stretch;
    glm::vec3 colorMultiplier;
    int currentFrame;
    float animationTimer;
    int totalFrames;
    float frameRate;
    Vector fieldOffset;
    Vector shakeOffset;

    Particle();
    void Reset();
    void Update();
    void UpdateAnimation();
};

#endif
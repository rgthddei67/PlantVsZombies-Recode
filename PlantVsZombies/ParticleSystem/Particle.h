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
    ZOMBIE_POLEVAULTER_ARM_OFF,
    ZOMBIE_CONE_OFF,
    ZOMBIE_BUCKET_OFF,
    NUM_EFFECTS，
};

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

    Particle();
    void Reset();
    void Update();
};

#endif
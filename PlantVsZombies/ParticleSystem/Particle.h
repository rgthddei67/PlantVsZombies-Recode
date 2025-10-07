#pragma once
#ifndef __PARTICLE_H__
#define __PARTICLE_H__

#include <SDL2/SDL.h>
#include <memory>
#include <vector>

enum class ParticleEffect {
    PEA_BULLET_HIT,
    ZOMBIE_HEAD_OFF,
    NUM_EFFECTS
};

// ������������
struct Particle {
    SDL_FPoint position;
    SDL_FPoint velocity;
    SDL_Color color;
    SDL_Color startColor;
    SDL_Color endColor;
    int lifetime;
    int maxLifetime;
    float size;
    float startSize;
    float endSize;
    float rotation;
    float rotationSpeed;
    float gravity;
    bool active;
    bool fadeOut;
    SDL_Texture* texture;    // ��������
    SDL_Rect textureRect;    // ����Դ����
    bool useTexture;         // �Ƿ�ʹ������

    Particle();
    void Reset();
    void Update();
};

#endif
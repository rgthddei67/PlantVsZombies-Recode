#pragma once
#ifndef __PARTICLE_CONFIG_H__
#define __PARTICLE_CONFIG_H__

#include "Particle.h"
#include <string>
#include <unordered_map>

struct ParticleConfig {
    SDL_Color startColor = { 255, 255, 255, 255 };
    SDL_Color endColor = { 255, 255, 255, 0 };
    int lifetime = 60;
    float startSize = 10.0f;
    float endSize = 5.0f;
    float minVelocity = 50.0f;
    float maxVelocity = 150.0f;
    float spreadAngle = 360.0f;
    float gravity = 0.5f;
    bool fadeOut = true;
    std::string textureKey;  // 纹理在ResourceManager中的key
    bool useTexture = false; // 是否使用纹理
    int textureFrame = 0;    // 纹理帧（如果有多帧）
};

class ParticleConfigManager {
private:
    std::unordered_map<ParticleEffect, ParticleConfig> configs;
    SDL_Renderer* renderer;

public:
    ParticleConfigManager(SDL_Renderer* sdlRenderer);
    void SetRenderer(SDL_Renderer* sdlRenderer) { renderer = sdlRenderer; }
    const ParticleConfig& GetConfig(ParticleEffect effect) const;
    SDL_Texture* GetTextureForEffect(ParticleEffect effect) const;

private:
    void InitializeConfigs();
};

#endif
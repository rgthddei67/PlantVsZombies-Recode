#pragma once
#ifndef __PARTICLE_EMITTER_H__
#define __PARTICLE_EMITTER_H__

#include "Particle.h"
#include "ParticleConfig.h"
#include <vector>

class ParticleEmitter {
private:
    SDL_Renderer* renderer;
    std::vector<Particle> particles;
    ParticleConfigManager configManager;

    SDL_FPoint position;
    bool active;
    float spawnTimer;
    int spawnRate;
    int maxParticles;

    // 发射控制
    ParticleType effectType;
    bool isOneShot;
    int particlesToEmit;
    int particlesEmitted;

    // 自动销毁
    float autoDestroyTimer;
    float autoDestroyTime;

public:
    ParticleEmitter(SDL_Renderer* sdlRenderer = nullptr);
    ~ParticleEmitter() = default;

    void SetRenderer(SDL_Renderer* sdlRenderer) 
    {
        renderer = sdlRenderer;
        configManager.SetRenderer(sdlRenderer);
    }

    // 初始化
    void Initialize(ParticleType type, const SDL_FPoint& pos);

    // 配置
    void SetSpawnRate(int rate) { spawnRate = rate; } 
    void SetMaxParticles(int max) { maxParticles = max; }
    void SetOneShot(bool oneShot) { isOneShot = oneShot; }
	// 如果frames为-1则永不自动销毁 若-2则在粒子全部消失后自动销毁
    void SetAutoDestroyTime(int frames);

    // 控制
    void EmitParticles(int count);
    void Stop() { active = false; }
    void Clear();

    // 状态查询
    bool IsActive() const { return active; }
    bool ShouldDestroy() const;
    int GetActiveParticleCount() const;
    void SetPosition(const SDL_FPoint& pos) { position = pos; }

    // 更新和渲染
    void Update();
    void Draw(SDL_Renderer* renderer);

private:
    void EmitSingleParticle();
    Particle* GetFreeParticle();
};

#endif
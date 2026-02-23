#pragma once
#ifndef __PARTICLE_SYSTEM_H__
#define __PARTICLE_SYSTEM_H__

#include "ParticleEmitter.h"
#include "../Game/Definit.h"
#include <vector>
#include <memory>

class ParticleSystem 
{
private:
    std::vector<std::unique_ptr<ParticleEmitter>> emitters;
    SDL_Renderer* renderer;

public:
    ParticleSystem(SDL_Renderer* Renderer);
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    // 系统管理
    void UpdateAll();
    void DrawAll();
    void ClearAll();

    // 发射粒子
    void EmitEffect(ParticleType type, const SDL_FPoint& position, int count = 1);

    // 发射粒子
    void EmitEffect(ParticleType type, float x, float y, int count = 1);

    // 发射粒子
    void EmitEffect(ParticleType type, const Vector& position, int count = 1);

    // 持续发射器
    ParticleEmitter* CreatePersistentEmitter(ParticleType type, const SDL_FPoint& position);
    void RemoveEmitter(ParticleEmitter* emitter);

    // 统计信息
    int GetTotalParticles() const;
    size_t GetActiveEmitters() const;

private:
    void CleanupInactiveEmitters();
};

extern std::unique_ptr<ParticleSystem> g_particleSystem;

#endif
#pragma once
#ifndef __PARTICLE_SYSTEM_H__
#define __PARTICLE_SYSTEM_H__

#include "ParticleEmitter.h"
#include "../Game/Definit.h"
#include "../Graphics.h"
#include <vector>
#include <memory>

class ParticleSystem {
private:
    std::vector<std::unique_ptr<ParticleEmitter>> emitters;
    Graphics* m_graphics;  

public:
    explicit ParticleSystem(Graphics* graphics);
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    void UpdateAll();
    void DrawAll();
    void ClearAll();

    // 发射一次性粒子效果
    void EmitEffect(ParticleType type, const Vector& position, int count = 1);

    // 创建持续发射器
    ParticleEmitter* CreatePersistentEmitter(ParticleType type, const Vector& position);
    void RemoveEmitter(ParticleEmitter* emitter);

    // 统计信息
    int GetTotalParticles() const;
    size_t GetActiveEmitters() const;

private:
    void CleanupInactiveEmitters();
};

extern std::unique_ptr<ParticleSystem> g_particleSystem;

#endif
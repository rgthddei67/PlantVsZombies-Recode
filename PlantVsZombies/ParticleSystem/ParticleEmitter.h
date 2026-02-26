#pragma once
#ifndef __PARTICLE_EMITTER_H__
#define __PARTICLE_EMITTER_H__

#include "Particle.h"
#include "ParticleConfig.h"
#include "../Graphics.h"
#include <vector>

class ParticleEmitter {
private:
    Graphics* m_graphics;                    
    std::vector<Particle> particles;
    ParticleConfigManager configManager;

    Vector position;                   
    bool active;
    float spawnTimer;
    int spawnRate;                            
    int maxParticles;

    ParticleType effectType;
    bool isOneShot;                             
    int particlesToEmit;                      
    int particlesEmitted;                     

    float autoDestroyTimer;                   
    float autoDestroyTime;         // 如果等于-2，则是自动销毁         

public:
    ParticleEmitter(Graphics* g = nullptr);
    ~ParticleEmitter() = default;

    void SetGraphics(Graphics* g) {
        m_graphics = g;
        configManager.SetGraphics(g);
    }

    void Initialize(ParticleType type, const Vector& pos);

    void SetSpawnRate(int rate) { spawnRate = rate; }
    void SetMaxParticles(int max) { maxParticles = max; }
    void SetOneShot(bool oneShot) { isOneShot = oneShot; }
    // -2为自动销毁 -1是永不销毁
    void SetAutoDestroyTime(float seconds);

    // 发射粒子
    void EmitParticles(int count);
    void Stop() { active = false; }
    void Clear();

    bool IsActive() const { return active; }
    bool ShouldDestroy() const;               
    int GetActiveParticleCount() const;
    void SetPosition(const Vector& pos) { position = pos; }

    void Update();
    void Draw();                           

private:
    void EmitSingleParticle();
    Particle* GetFreeParticle();
};

#endif
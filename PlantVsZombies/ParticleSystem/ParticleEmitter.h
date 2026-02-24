#pragma once
#ifndef __PARTICLE_EMITTER_H__
#define __PARTICLE_EMITTER_H__

#include "Particle.h"
#include "ParticleConfig.h"
#include "../Graphics.h"
#include <vector>

class ParticleEmitter {
private:
    Graphics* m_graphics;                     // 渲染目标
    std::vector<Particle> particles;
    ParticleConfigManager configManager;

    Vector position;                       // 发射器世界坐标
    bool active;
    float spawnTimer;
    int spawnRate;                             // 每秒发射次数
    int maxParticles;

    ParticleType effectType;
    bool isOneShot;                             // 一次性发射
    int particlesToEmit;                         // 需要发射的粒子总数
    int particlesEmitted;                         // 已发射粒子数

    float autoDestroyTimer;                       // 自动销毁计时
    float autoDestroyTime;                         // 自动销毁时间（秒），<=0 表示不自动销毁

public:
    ParticleEmitter(Graphics* g = nullptr);
    ~ParticleEmitter() = default;

    void SetGraphics(Graphics* g) {
        m_graphics = g;
        configManager.SetGraphics(g);
    }

    // 初始化
    void Initialize(ParticleType type, const Vector& pos);

    // 配置
    void SetSpawnRate(int rate) { spawnRate = rate; }
    void SetMaxParticles(int max) { maxParticles = max; }
    void SetOneShot(bool oneShot) { isOneShot = oneShot; }
    // frames 为 -2 表示粒子全部消失后自动销毁；-1 永不销毁；其他值表示经过 frames 秒后销毁
    void SetAutoDestroyTime(float seconds);

    // 控制
    void EmitParticles(int count);
    void Stop() { active = false; }
    void Clear();

    // 状态查询
    bool IsActive() const { return active; }
    bool ShouldDestroy() const;                // 是否可以销毁
    int GetActiveParticleCount() const;
    void SetPosition(const Vector& pos) { position = pos; }

    // 更新和渲染
    void Update();
    void Draw();                               // 使用 m_graphics 进行绘制

private:
    void EmitSingleParticle();
    Particle* GetFreeParticle();
};

#endif
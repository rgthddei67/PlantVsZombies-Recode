#pragma once
#ifndef __PARTICLE_EMITTER_H__
#define __PARTICLE_EMITTER_H__

#include "Particle.h"
#include "ParticleConfig.h"
#include "../Graphics.h"
#include <vector>

class ParticleEmitter {
private:
    Graphics* m_graphics;                     // 娓叉煋鐩爣
    std::vector<Particle> particles;
    ParticleConfigManager configManager;

    Vector position;                       // 鍙戝皠鍣ㄤ笘鐣屽潗鏍?
    bool active;
    float spawnTimer;
    int spawnRate;                             // 姣忕鍙戝皠娆℃暟
    int maxParticles;

    ParticleType effectType;
    bool isOneShot;                             // 涓€娆℃€у彂灏?
    int particlesToEmit;                         // 闇€瑕佸彂灏勭殑绮掑瓙鎬绘暟
    int particlesEmitted;                         // 宸插彂灏勭矑瀛愭暟

    float autoDestroyTimer;                       // 鑷姩閿€姣佽鏃?
    float autoDestroyTime;                         // 鑷姩閿€姣佹椂闂达紙绉掞級锛?=0 琛ㄧず涓嶈嚜鍔ㄩ攢姣?

public:
    ParticleEmitter(Graphics* g = nullptr);
    ~ParticleEmitter() = default;

    void SetGraphics(Graphics* g) {
        m_graphics = g;
        configManager.SetGraphics(g);
    }

    // 鍒濆鍖?
    void Initialize(ParticleType type, const Vector& pos);

    // 閰嶇疆
    void SetSpawnRate(int rate) { spawnRate = rate; }
    void SetMaxParticles(int max) { maxParticles = max; }
    void SetOneShot(bool oneShot) { isOneShot = oneShot; }
    // frames 涓?-2 琛ㄧず绮掑瓙鍏ㄩ儴娑堝け鍚庤嚜鍔ㄩ攢姣侊紱-1 姘镐笉閿€姣侊紱鍏朵粬鍊艰〃绀虹粡杩?frames 绉掑悗閿€姣?
    void SetAutoDestroyTime(float seconds);

    // 鎺у埗
    void EmitParticles(int count);
    void Stop() { active = false; }
    void Clear();

    // 鐘舵€佹煡璇?
    bool IsActive() const { return active; }
    bool ShouldDestroy() const;                // 鏄惁鍙互閿€姣?
    int GetActiveParticleCount() const;
    void SetPosition(const Vector& pos) { position = pos; }

    // 鏇存柊鍜屾覆鏌?
    void Update();
    void Draw();                               // 浣跨敤 m_graphics 杩涜缁樺埗

private:
    void EmitSingleParticle();
    Particle* GetFreeParticle();
};

#endif
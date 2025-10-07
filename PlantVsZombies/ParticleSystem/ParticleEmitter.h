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
    int spawnTimer;
    int spawnRate;
    int maxParticles;

    // �������
    ParticleEffect effectType;
    bool isOneShot;
    int particlesToEmit;
    int particlesEmitted;

    // �Զ�����
    int autoDestroyTimer;
    int autoDestroyTime;

public:
    ParticleEmitter(SDL_Renderer* sdlRenderer = nullptr);
    ~ParticleEmitter() = default;

    void SetRenderer(SDL_Renderer* sdlRenderer) 
    {
        renderer = sdlRenderer;
        configManager.SetRenderer(sdlRenderer);
    }

    // ��ʼ��
    void Initialize(ParticleEffect type, const SDL_FPoint& pos);

    // ����
    void SetSpawnRate(int rate) { spawnRate = rate; } 
    void SetMaxParticles(int max) { maxParticles = max; }
    void SetOneShot(bool oneShot) { isOneShot = oneShot; }
	// ���framesΪ-1�������Զ����� ��-2��������ȫ����ʧ���Զ�����
    void SetAutoDestroyTime(int frames);

    // ����
    void EmitParticles(int count);
    void Stop() { active = false; }
    void Clear();

    // ״̬��ѯ
    bool IsActive() const { return active; }
    bool ShouldDestroy() const;
    int GetActiveParticleCount() const;
    void SetPosition(const SDL_FPoint& pos) { position = pos; }

    // ���º���Ⱦ
    void Update();
    void Draw(SDL_Renderer* renderer);

private:
    void EmitSingleParticle();
    Particle* GetFreeParticle();
};

#endif
#pragma once
#ifndef __PARTICLE_SYSTEM_H__
#define __PARTICLE_SYSTEM_H__

#include "ParticleEmitter.h"
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

    // ϵͳ����
    void UpdateAll();
    void DrawAll();
    void ClearAll();

    // ����Ч��
    void EmitEffect(ParticleEffect type, const SDL_FPoint& position, int count = 1);
    void EmitEffect(ParticleEffect type, float x, float y, int count = 1);

    // ����������
    ParticleEmitter* CreatePersistentEmitter(ParticleEffect type, const SDL_FPoint& position);
    void RemoveEmitter(ParticleEmitter* emitter);

    // ͳ����Ϣ
    int GetTotalParticles() const;
    int GetActiveEmitters() const;

private:
    void CleanupInactiveEmitters();
};

#endif
#include "ParticleEmitter.h"
#include "../DeltaTime.h"
#include <iostream>
#include <cmath>
#include <cstdlib>

ParticleEmitter::ParticleEmitter(SDL_Renderer* sdlRenderer)
    : active(false), spawnTimer(0), spawnRate(0), maxParticles(100),
    configManager(sdlRenderer),
    isOneShot(false), particlesToEmit(0), particlesEmitted(0),
    autoDestroyTimer(0), autoDestroyTime(-2),
    effectType(ParticleEffect::PEA_BULLET_HIT)
{
    particles.resize(maxParticles);
}

void ParticleEmitter::Initialize(ParticleEffect type, const SDL_FPoint& pos) 
{
    effectType = type;
    position = pos;
    active = true;
    spawnTimer = 0;
    particlesEmitted = 0;
}

void ParticleEmitter::SetAutoDestroyTime(int frames) {
    if (frames == -2) {
        const ParticleConfig& config = configManager.GetConfig(effectType);
        autoDestroyTime = config.lifetime + 15;
    }
    else {
        autoDestroyTime = frames;
    }
    autoDestroyTimer = 0;
}

void ParticleEmitter::Update() {
    if (!active) return;

    float deltaTime = DeltaTime::GetDeltaTime();

    // 自动销毁计时
    if (autoDestroyTime > 0) {
        autoDestroyTimer += deltaTime;
        if (autoDestroyTimer >= autoDestroyTime) {
            active = false;
            return;
        }
    }

    // 一次性发射控制
    if (isOneShot && particlesEmitted >= particlesToEmit) {
        spawnRate = 0;
        if (GetActiveParticleCount() == 0) {
            active = false;
        }
    }

    // 自动发射
    if (spawnRate > 0 && (!isOneShot || particlesEmitted < particlesToEmit)) {
        spawnTimer += deltaTime;
        float spawnInterval = 1.0f / spawnRate;
        if (spawnTimer >= spawnInterval) {
            if (!isOneShot || particlesEmitted < particlesToEmit) {
                EmitSingleParticle();
                spawnTimer = 0;
                particlesEmitted++;
            }
        }
    }

    // 更新所有粒子
    for (auto& particle : particles) {
        if (particle.active) {
            particle.Update();
        }
    }
}

void ParticleEmitter::EmitParticles(int count) {
    if (isOneShot) {
        particlesToEmit = count;
        particlesEmitted = 0;
    }

    for (int i = 0; i < count && (!isOneShot || particlesEmitted < particlesToEmit); ++i) {
        EmitSingleParticle();
        if (isOneShot) {
            particlesEmitted++;
        }
    }
}

void ParticleEmitter::EmitSingleParticle() {
    Particle* particle = GetFreeParticle();
    if (!particle) return;
    const ParticleConfig& config = configManager.GetConfig(effectType);

    particle->Reset();
    particle->active = true;
    particle->position = position;
    particle->maxLifetime = config.lifetime;
    particle->startColor = config.startColor;
    particle->endColor = config.endColor;
    particle->startSize = config.startSize;
    particle->endSize = config.endSize;
    particle->fadeOut = config.fadeOut;
    particle->gravity = config.gravity;

    particle->useTexture = config.useTexture;
    if (config.useTexture) 
    {
        particle->texture = configManager.GetRandomTextureForEffect(effectType);
        if (particle->texture) 
        {
            int w, h;
            SDL_QueryTexture(particle->texture, NULL, NULL, &w, &h);
            particle->textureRect = { 0, 0, w, h };
        }
    }

    // 随机速度
    float baseAngle = 270.0f; // 默认向下
    float angleVariation = config.spreadAngle / 2.0f;
    float angle = (baseAngle - angleVariation) +
        (static_cast<float>(rand()) / RAND_MAX * config.spreadAngle);
    angle = angle * 3.14159f / 180.0f;

    float speed = config.minVelocity +
        static_cast<float>(rand()) / RAND_MAX * (config.maxVelocity - config.minVelocity);

    particle->velocity.x = cosf(angle) * speed;
    particle->velocity.y = sinf(angle) * speed;

    particle->rotationSpeed = (rand() % 100 - 50) * 0.1f;
}

Particle* ParticleEmitter::GetFreeParticle() 
{
    for (auto& particle : particles) {
        if (!particle.active) {
            return &particle;
        }
    }
    return nullptr;
}

void ParticleEmitter::Draw(SDL_Renderer* renderer) 
{
    for (const auto& particle : particles) {
        if (particle.active) {
            if (particle.useTexture && particle.texture) {
                // 使用纹理渲染
                SDL_FRect destRect = {
                    particle.position.x - particle.size * 0.5f,
                    particle.position.y - particle.size * 0.5f,
                    particle.size,
                    particle.size
                };

                // 设置颜色调制（让纹理受粒子颜色影响）
                SDL_SetTextureColorMod(particle.texture,
                    particle.color.r, particle.color.g, particle.color.b);
                SDL_SetTextureAlphaMod(particle.texture, particle.color.a);

                // 渲染纹理（带旋转）
                SDL_RenderCopyExF(renderer, particle.texture,
                    &particle.textureRect, &destRect,
                    particle.rotation, NULL, SDL_FLIP_NONE);
            }
            else {
                SDL_FRect rect = {
                    particle.position.x - particle.size * 0.5f,
                    particle.position.y - particle.size * 0.5f,
                    particle.size,
                    particle.size
                };

                SDL_SetRenderDrawColor(renderer,
                    particle.color.r, particle.color.g, particle.color.b, particle.color.a);
                SDL_RenderFillRectF(renderer, &rect);
            }
        }
    }
}

void ParticleEmitter::Clear() {
    for (auto& particle : particles) {
        particle.active = false;
    }
}

bool ParticleEmitter::ShouldDestroy() const {
    return !active && GetActiveParticleCount() == 0;
}

int ParticleEmitter::GetActiveParticleCount() const {
    int count = 0;
    for (const auto& particle : particles) {
        if (particle.active) count++;
    }
    return count;
}
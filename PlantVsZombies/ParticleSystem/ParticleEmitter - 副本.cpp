#include "ParticleEmitter.h"
#include "../DeltaTime.h"
#include "../Game/Definit.h"
#include "../GameApp.h"
#include <iostream>
#include <cmath>
#include <cstdlib>

ParticleEmitter::ParticleEmitter(SDL_Renderer* sdlRenderer)
    : active(false), spawnTimer(0.0f), spawnRate(0), maxParticles(100),
    configManager(sdlRenderer),
    isOneShot(false), particlesToEmit(0), particlesEmitted(0),
    autoDestroyTimer(0.0f), autoDestroyTime(-2.0f),
    effectType(ParticleType::PEA_BULLET_HIT)
{
    particles.resize(maxParticles);
}

void ParticleEmitter::Initialize(ParticleType type, const SDL_FPoint& pos) 
{
    effectType = type;
    position = pos;
    active = true;
    spawnTimer = 0.0f;
    particlesEmitted = 0;
}

void ParticleEmitter::SetAutoDestroyTime(int frames) {
    if (frames == -2) {
        const ParticleConfig& config = configManager.GetConfig(effectType);
        autoDestroyTime = config.lifetime;
    }
    else {
        autoDestroyTime = static_cast<float>(frames);
    }
    autoDestroyTimer = 0.0f;
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
    for (size_t i = 0; i < particles.size(); i++)
    {
        auto particle = particles[i];
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

    // 随机角度（度），然后转换为弧度
    float randomAngleDeg = GameRandom::Range(0.0f, config.spreadAngle);
    float randomAngleRad = randomAngleDeg * (3.14159f / 180.0f);

    // 随机速度
    float randomSpeed = GameRandom::Range(config.minVelocity, config.maxVelocity);

    // 计算速度向量
    float cosAngle = cosf(randomAngleRad);
    float sinAngle = sinf(randomAngleRad);
    particle->velocity = Vector(cosAngle * randomSpeed, sinAngle * randomSpeed);

    // 随机旋转速度（-5 到 5 度/秒）
    particle->rotationSpeed = GameRandom::Range(-5.0f, 5.0f);
}

Particle* ParticleEmitter::GetFreeParticle()
{
    for (size_t i = 0; i < particles.size(); ++i)
    {
        if (!particles[i].active)
            return &particles[i]; 
    }
    return nullptr;
}

void ParticleEmitter::Draw(SDL_Renderer* renderer) 
{
    for (size_t i = 0; i < particles.size(); i++)
    {
        auto particle = particles[i];
        if (particle.active) {
            if (particle.useTexture && particle.texture) {
                // 获取源纹理区域的尺寸
                float srcW = static_cast<float>(particle.textureRect.w);
                float srcH = static_cast<float>(particle.textureRect.h);

                // 目标尺寸 = 原始尺寸 × size（缩放倍数）
                float destW = srcW * particle.size;
                float destH = srcH * particle.size;

                Vector screenPositon = GameAPP::GetInstance().GetCamera().WorldToScreen
                (Vector(particle.position.x - destW * 0.5f,
                   particle.position.y - destH * 0.5f));

                // 计算目标矩形（以粒子位置为中心）
                SDL_FRect destRect = {
                    screenPositon.x,
                    screenPositon.y,
                    destW,
                    destH
                };

                // 设置颜色调制（让纹理受粒子颜色影响）
                SDL_SetTextureColorMod(particle.texture,
                    particle.color.r, particle.color.g, particle.color.b);
                SDL_SetTextureAlphaMod(particle.texture, particle.color.a);

                // 渲染纹理
                SDL_RenderCopyExF(renderer, particle.texture,
                    &particle.textureRect, &destRect,
                    particle.rotation, NULL, SDL_FLIP_NONE);
            }
            else {
                Vector screenPositon = GameAPP::GetInstance().GetCamera().WorldToScreen
                (Vector(particle.position.x - particle.size * 0.5f,
                    particle.position.y - particle.size * 0.5f));
                // 无纹理粒子：保持正方形，size 作为边长
                SDL_FRect rect = {
                    screenPositon.x,
                    screenPositon.y,
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
    for (size_t i = 0; i < particles.size(); i++)
    {
        particles[i].active = false;
    }
}

bool ParticleEmitter::ShouldDestroy() const {
    return !active || GetActiveParticleCount() == 0;
}

int ParticleEmitter::GetActiveParticleCount() const {
    int count = 0;
    for (size_t i = 0; i < particles.size(); i++)
    {
        auto particle = particles[i];
        if (particle.active) count++;
    }
    return count;
}
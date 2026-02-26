#include "ParticleEmitter.h"
#include "../DeltaTime.h"
#include "../Game/Definit.h"
#include "../GameRandom.h"
#include <iostream>
#include <cmath>

ParticleEmitter::ParticleEmitter(Graphics* g)
    : m_graphics(g)
    , active(false)
    , spawnTimer(0.0f)
    , spawnRate(0)
    , maxParticles(100)
    , configManager(g)
    , isOneShot(false)
    , particlesToEmit(0)
    , particlesEmitted(0)
    , autoDestroyTimer(0.0f)
    , autoDestroyTime(-2.0f)
    , effectType(ParticleType::PEA_BULLET_HIT)
{
    particles.resize(maxParticles);
}

void ParticleEmitter::Initialize(ParticleType type, const Vector& pos) {
    effectType = type;
    position = pos;
    active = true;
    spawnTimer = 0.0f;
    particlesEmitted = 0;
}

void ParticleEmitter::SetAutoDestroyTime(float seconds) {
    if (seconds == -2.0f) {
        const ParticleConfig& config = configManager.GetConfig(effectType);
        autoDestroyTime = config.lifetime;
    }
    else {
        autoDestroyTime = seconds;
    }
    autoDestroyTimer = 0.0f;
}

void ParticleEmitter::Update() {
    if (!active) return;

    float deltaTime = DeltaTime::GetDeltaTime();

    // 自动销毁
    if (autoDestroyTime > 0) {
        autoDestroyTimer += deltaTime;
        if (autoDestroyTimer >= autoDestroyTime) {
            active = false;
        }
    }

    // 是否发射完毕
    if (isOneShot && particlesEmitted >= particlesToEmit) {
        spawnRate = 0;
        if (GetActiveParticleCount() == 0) {
            active = false;
        }
    }

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

    for (size_t i = 0; i < particles.size(); i++)
    {
        Particle& particle = particles[i];
        if (particle.active)
        {
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

    particle->texture = configManager.GetRandomTextureForEffect(effectType);

    float randomAngleDeg = GameRandom::Range(0.0f, config.spreadAngle);
    float randomAngleRad = randomAngleDeg * (3.14159f / 180.0f);

    float randomSpeed = GameRandom::Range(config.minVelocity, config.maxVelocity);
    particle->velocity = Vector(
        cosf(randomAngleRad) * randomSpeed,
        sinf(randomAngleRad) * randomSpeed
    );

    particle->rotationSpeed = GameRandom::Range(-5.0f, 5.0f);
}

Particle* ParticleEmitter::GetFreeParticle() {
    for (auto& particle : particles) {
        if (!particle.active)
            return &particle;
    }
    return nullptr;
}

void ParticleEmitter::Draw() {
    if (!m_graphics) return;

    for (size_t i = 0; i < particles.size(); i++)
    {
        Particle& particle = particles[i];
        if (particle.active)
        {
            if (!particle.texture) {
                std::cerr << "ParticleEmitter::Draw: 没有图片绘制" << std::endl;
                continue;
            }
            float srcW = static_cast<float>(particle.texture->width);
            float srcH = static_cast<float>(particle.texture->height);
            float destW = srcW * particle.size;
            float destH = srcH * particle.size;

            // 鐩爣鐭╁舰涓績瀵归綈绮掑瓙浣嶇疆
            float x = particle.position.x - destW * 0.5f;
            float y = particle.position.y - destH * 0.5f;

            m_graphics->DrawTexture(
                particle.texture,
                x, y, destW, destH,
                particle.rotation,
                particle.color
            );
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
    return !active && GetActiveParticleCount() == 0;
}

int ParticleEmitter::GetActiveParticleCount() const {
    int count = 0;
    for (size_t i = 0; i < particles.size(); i++)
    {
        if (particles[i].active)
        {
            count++;
        }
    }
    return count;
}
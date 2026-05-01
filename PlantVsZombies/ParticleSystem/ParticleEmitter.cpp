#include "ParticleEmitter.h"
#include "../DeltaTime.h"
#include "../Game/Definit.h"
#include "../GameRandom.h"
#include "../ResourceManager.h"
#include <iostream>
#include <cmath>
#include <algorithm>

ParticleEmitter::ParticleEmitter(Graphics* g)
    : m_graphics(g)
    , active(false)
    , spawnTimer(0.0f)
    , spawnRate(0)
    , maxParticles(0)
    , isOneShot(false)
    , particlesToEmit(0)
    , particlesEmitted(0)
    , systemTimer(0.0f)
{
}

void ParticleEmitter::Initialize(const EmitterConfig& config, const Vector& pos) {
    xmlConfig = config;
    position = pos;
    active = true;
    spawnTimer = 0.0f;
    particlesEmitted = 0;
    systemTimer = 0.0f;

    spawnRate = config.spawnRate;

    isOneShot = true;
    particlesToEmit = config.spawnMaxLaunched;

    maxParticles = std::max(config.spawnMaxLaunched, config.spawnMinActive);
    particles.resize(maxParticles);

    for (int i = 0; i < config.spawnMinActive; i++) {
        EmitSingleParticle();
    }

    activeFields = config.fields;
}

void ParticleEmitter::Update() {
    float deltaTime = DeltaTime::GetDeltaTime();

    if (active) {
        systemTimer += deltaTime;

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
    }

    for (size_t i = 0; i < particles.size(); i++)
    {
        Particle& particle = particles[i];
        if (particle.active)
        {
            float normalizedTime = particle.lifetime / particle.maxLifetime;

            float systemAlphaValue = 1.0f;
            if (xmlConfig.systemDuration > 0.0f) {
                float systemNormalizedTime = systemTimer / xmlConfig.systemDuration;
                systemAlphaValue = xmlConfig.systemAlpha.GetValue(systemNormalizedTime);
            }

            float alpha = xmlConfig.particleAlpha.GetValue(normalizedTime);
            float scale = xmlConfig.particleScale.GetValue(normalizedTime);
            float stretch = xmlConfig.particleStretch.GetValue(normalizedTime);

            particle.size = scale;
            particle.stretch = stretch;
            particle.color.a = alpha * systemAlphaValue * 255.0f;

            float red = xmlConfig.particleRed.GetValue(normalizedTime);
            float green = xmlConfig.particleGreen.GetValue(normalizedTime);
            float blue = xmlConfig.particleBlue.GetValue(normalizedTime);
            particle.colorMultiplier = glm::vec3(red, green, blue);

            for (const ParticleField& field : activeFields) {
                float xValue = field.xTrack.GetValue(normalizedTime);
                float yValue = field.yTrack.GetValue(normalizedTime);

                if (field.type == ParticleFieldType::POSITION) {
                    particle.fieldOffset.x = xValue;
                    particle.fieldOffset.y = yValue;
                }
                else if (field.type == ParticleFieldType::SHAKE) {
                    particle.shakeOffset.x = GameRandom::Range(-xValue, xValue);
                    particle.shakeOffset.y = GameRandom::Range(-yValue, yValue);
                }
                else if (field.type == ParticleFieldType::FRICTION) {
                    particle.velocity.x *= (1.0f - xValue);
                    particle.velocity.y *= (1.0f - yValue);
                }
            }

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

    particle->Reset();
    particle->active = true;

    particle->position = GetSpawnPosition();

    particle->maxLifetime = xmlConfig.particleDuration.GetRandomValue();

    float speed = xmlConfig.launchSpeed.GetRandomValue();
    float angle = 0.0f;
    if (xmlConfig.randomLaunchSpin) {
        angle = GameRandom::Range(0.0f, 360.0f) * (3.14159f / 180.0f);
    }
    particle->velocity = Vector(cosf(angle) * speed, sinf(angle) * speed);

    particle->rotationSpeed = xmlConfig.particleSpinSpeed.GetRandomValue();

    particle->gravity = xmlConfig.particleGravity;

    if (!xmlConfig.imageKeys.empty()) {
        ResourceManager& resourceManager = ResourceManager::GetInstance();
        int randomIndex = GameRandom::Range(0, static_cast<int>(xmlConfig.imageKeys.size()) - 1);
        particle->texture = resourceManager.GetTexture(xmlConfig.imageKeys[randomIndex]);
    }

    particle->totalFrames = xmlConfig.imageFrames;
    particle->frameRate = xmlConfig.animationRate;
    particle->currentFrame = 0;
    particle->animationTimer = 0.0f;

    particle->brightness = xmlConfig.particleBrightness.GetRandomValue();

    particle->size = xmlConfig.particleScale.GetValue(0.0f);

    particle->color = glm::vec4(255.0f);
}

Vector ParticleEmitter::GetSpawnPosition() const {
    Vector spawnPos = position;

    switch (xmlConfig.emitterType) {
    case EmitterType::POINT:
        break;

    case EmitterType::BOX:
        spawnPos.x += xmlConfig.emitterBoxX.GetRandomValue();
        spawnPos.y += xmlConfig.emitterBoxY.GetRandomValue();
        break;

    case EmitterType::CIRCLE: {
        float radius = xmlConfig.emitterRadius.GetRandomValue();
        float angle = GameRandom::Range(0.0f, 360.0f) * (3.14159f / 180.0f);
        spawnPos.x += cosf(angle) * radius;
        spawnPos.y += sinf(angle) * radius;
        break;
    }
    }

    return spawnPos;
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
            float destH = srcH * particle.size * particle.stretch;

            float x = particle.position.x + particle.fieldOffset.x + particle.shakeOffset.x - destW * 0.5f;
            float y = particle.position.y + particle.fieldOffset.y + particle.shakeOffset.y - destH * 0.5f;

            glm::vec4 finalColor = particle.color;
            finalColor.r *= particle.brightness * particle.colorMultiplier.r;
            finalColor.g *= particle.brightness * particle.colorMultiplier.g;
            finalColor.b *= particle.brightness * particle.colorMultiplier.b;

            m_graphics->DrawTexture(
                particle.texture,
                x, y, destW, destH,
                particle.rotation,
                finalColor
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

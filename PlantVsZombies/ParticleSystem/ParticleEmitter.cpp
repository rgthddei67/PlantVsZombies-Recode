#include "ParticleEmitter.h"
#include "../DeltaTime.h"
#include "../Game/Definit.h"
#include "../GameRandom.h"
#include "../ResourceManager.h"
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
    , systemTimer(0.0f)
{
    particles.resize(maxParticles);
}

void ParticleEmitter::Initialize(const EmitterConfig& config, const Vector& pos) {
    xmlConfig = config;
    position = pos;
    active = true;
    spawnTimer = 0.0f;
    particlesEmitted = 0;
    systemTimer = 0.0f;

    // 设置发射速率
    spawnRate = config.spawnRate;

    // 设置为一次性发射
    isOneShot = true;
    particlesToEmit = config.spawnMaxLaunched;

    // 立即发射最小活跃粒子数
    for (int i = 0; i < config.spawnMinActive; i++) {
        EmitSingleParticle();
    }

    // 设置自动销毁时间
    if (config.systemDuration > 0.0f) {
        autoDestroyTime = config.systemDuration;
    }
    else {
        autoDestroyTime = -1.0f;  // 永不销毁
    }

    activeFields = config.fields;
}

void ParticleEmitter::SetAutoDestroyTime(float seconds) {
    autoDestroyTime = seconds;
    autoDestroyTimer = 0.0f;
}

void ParticleEmitter::Update() {
    if (!active) return;

    float deltaTime = DeltaTime::GetDeltaTime();

    systemTimer += deltaTime;

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

    // 更新粒子
    for (size_t i = 0; i < particles.size(); i++)
    {
        Particle& particle = particles[i];
        if (particle.active)
        {
            // 应用XML场效果
            float normalizedTime = particle.lifetime / particle.maxLifetime;

            // 计算系统透明度
            float systemAlphaValue = 1.0f;
            if (autoDestroyTime > 0.0f) {
                float systemNormalizedTime = systemTimer / autoDestroyTime;
                systemAlphaValue = xmlConfig.systemAlpha.GetValue(systemNormalizedTime);
            }

            // 应用插值轨迹
            float alpha = xmlConfig.particleAlpha.GetValue(normalizedTime);
            float scale = xmlConfig.particleScale.GetValue(normalizedTime);
            float stretch = xmlConfig.particleStretch.GetValue(normalizedTime);

            particle.size = scale;
            particle.stretch = stretch;
            particle.color.a = alpha * systemAlphaValue * 255.0f;

            // 应用颜色乘数
            float red = xmlConfig.particleRed.GetValue(normalizedTime);
            float green = xmlConfig.particleGreen.GetValue(normalizedTime);
            float blue = xmlConfig.particleBlue.GetValue(normalizedTime);
            particle.colorMultiplier = glm::vec3(red, green, blue);

            // 应用场效果
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
                    // 应用摩擦力：速度乘以 (1 - 摩擦值)
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

    // 根据发射器形状计算生成位置
    particle->position = GetSpawnPosition();

    // 基础属性
    particle->maxLifetime = xmlConfig.particleDuration.GetRandomValue();

    // 发射速度
    float speed = xmlConfig.launchSpeed.GetRandomValue();
    float angle = 0.0f;
    if (xmlConfig.randomLaunchSpin) {
        angle = GameRandom::Range(0.0f, 360.0f) * (3.14159f / 180.0f);
    }
    particle->velocity = Vector(cosf(angle) * speed, sinf(angle) * speed);

    // 旋转速度
    particle->rotationSpeed = xmlConfig.particleSpinSpeed.GetRandomValue();

    // 重力
    particle->gravity = xmlConfig.particleGravity;

    // 纹理
    if (!xmlConfig.imageKeys.empty()) {
        ResourceManager& resourceManager = ResourceManager::GetInstance();
        int randomIndex = GameRandom::Range(0, static_cast<int>(xmlConfig.imageKeys.size()) - 1);
        particle->texture = resourceManager.GetTexture(xmlConfig.imageKeys[randomIndex]);
    }

    // 动画
    particle->totalFrames = xmlConfig.imageFrames;
    particle->frameRate = xmlConfig.animationRate;
    particle->currentFrame = 0;
    particle->animationTimer = 0.0f;

    // 亮度
    particle->brightness = xmlConfig.particleBrightness.GetRandomValue();

    // 初始大小（从插值轨迹获取）
    particle->startSize = xmlConfig.particleScale.GetValue(0.0f);
    particle->size = particle->startSize;

    // 颜色（默认白色）
    particle->color = glm::vec4(255.0f);
    particle->startColor = glm::vec4(255.0f);
    particle->endColor = glm::vec4(255.0f);

    // 应用场效果
    ApplyFieldsToParticle(particle);
}

Vector ParticleEmitter::GetSpawnPosition() const {
    Vector spawnPos = position;

    switch (xmlConfig.emitterType) {
    case EmitterType::POINT:
        // 点发射器，直接使用位置
        break;

    case EmitterType::BOX:
        // 盒子发射器，在矩形区域内随机
        spawnPos.x += xmlConfig.emitterBoxX.GetRandomValue();
        spawnPos.y += xmlConfig.emitterBoxY.GetRandomValue();
        break;

    case EmitterType::CIRCLE: {
        // 圆形发射器，在圆形区域内随机
        float radius = xmlConfig.emitterRadius.GetRandomValue();
        float angle = GameRandom::Range(0.0f, 360.0f) * (3.14159f / 180.0f);
        spawnPos.x += cosf(angle) * radius;
        spawnPos.y += sinf(angle) * radius;
        break;
    }
    }

    return spawnPos;
}

void ParticleEmitter::ApplyFieldsToParticle(Particle* particle) {
    // 场效果将在粒子更新时应用
    // 这里只是初始化
    particle->fieldOffset = { 0, 0 };
    particle->shakeOffset = { 0, 0 };
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

            // 应用亮度和颜色乘数
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
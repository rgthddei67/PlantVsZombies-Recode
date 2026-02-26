#include "ParticleConfig.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include <cmath>

ParticleConfigManager::ParticleConfigManager(Graphics* graphics)
    : m_graphics(graphics) {
    InitializeConfigs();
}

void ParticleConfigManager::InitializeConfigs() {
    ResourceManager& resourceManager = ResourceManager::GetInstance();

    ParticleConfig config;
    config.startColor = glm::vec4(1.0f);
    config.endColor = glm::vec4(1.0f);
    config.lifetime = 0.8f;
    config.startSize = 1.3f;
    config.endSize = 0.4f;
    config.minVelocity = 10.0f;
    config.maxVelocity = 15.0f;
    config.spreadAngle = 90.0f;
    config.gravity = 100.0f;
    config.textureKeys = {
        ResourceKeys::Particles::PARTICLE_PEA_HIT_0,
        ResourceKeys::Particles::PARTICLE_PEA_HIT_1,
        ResourceKeys::Particles::PARTICLE_PEA_HIT_2,
        ResourceKeys::Particles::PARTICLE_PEA_HIT_3
    };
    configs[ParticleType::PEA_BULLET_HIT] = config;

    config.startColor = glm::vec4(1.0f);
    config.endColor = glm::vec4(1.0f);
    config.lifetime = 0.8f;
    config.startSize = 0.75f;
    config.endSize = 0.75f;
    config.minVelocity = 60.0f;
    config.maxVelocity = 110.0f;
    config.spreadAngle = 100.0f;
    config.gravity = 120.0f;
    config.textureKeys = { ResourceKeys::Particles::PARTICLE_ZOMBIE_HEAD };
    configs[ParticleType::ZOMBIE_HEAD_OFF] = config;

    config.startColor = glm::vec4(1.0f);
    config.endColor = glm::vec4(1.0f);
    config.lifetime = 0.8f;
    config.startSize = 0.7f;
    config.endSize = 0.7f;
    config.minVelocity = 0.0f;
    config.maxVelocity = 0.0f;
    config.spreadAngle = 100.0f;
    config.gravity = 110.0f;
    config.textureKeys = { ResourceKeys::Particles::PARTICLE_ZOMBIEARM };
    configs[ParticleType::ZOMBIE_ARM_OFF] = config;

    config.startColor = glm::vec4(1.0f);
    config.endColor = glm::vec4(1.0f);
    config.lifetime = 0.8f;
    config.startSize = 0.8f;
    config.endSize = 0.8f;
    config.minVelocity = 70.0f;
    config.maxVelocity = 110.0f;
    config.spreadAngle = 140.0f;
    config.gravity = 130.0f;
    config.textureKeys = { "IMAGE_ZOMBIE_CONE3" };
    configs[ParticleType::ZOMBIE_CONE_OFF] = config;
}

const ParticleConfig& ParticleConfigManager::GetConfig(ParticleType effect) const {
    auto it = configs.find(effect);
    if (it != configs.end()) {
        return it->second;
    }
    static ParticleConfig defaultConfig;
    return defaultConfig;
}

const GLTexture* ParticleConfigManager::GetRandomTextureForEffect(ParticleType effect) const {
    const ParticleConfig& config = GetConfig(effect);

    ResourceManager& resourceManager = ResourceManager::GetInstance();
    std::string textureKey = config.GetRandomTextureKey();
    if (textureKey.empty()) return nullptr;

    return resourceManager.GetTexture(textureKey);
}
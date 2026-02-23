#include "../ParticleSystem/ParticleConfig.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include <cmath>

ParticleConfigManager::ParticleConfigManager(SDL_Renderer* sdlRenderer)
    : renderer(sdlRenderer) {
    InitializeConfigs();
}

ParticleConfigManager::~ParticleConfigManager()
{
    renderer = nullptr;
}

void ParticleConfigManager::InitializeConfigs() 
{
    ResourceManager& resourceManager = ResourceManager::GetInstance();
    // 子弹击中
    ParticleConfig config;
    config.startColor = { 255, 255, 255, 255 }; 
    config.endColor = { 255, 255, 255, 255 };
    config.lifetime = 0.8f;
    config.startSize = 1.3f;
    config.endSize = 0.4f;
    config.minVelocity = 10.0f;
    config.maxVelocity = 15.0f;
    config.spreadAngle = 90.0f;
    config.gravity = 100.0f;
    config.textureKeys = 
    { ResourceKeys::Particles::PARTICLE_PEA_HIT_0, ResourceKeys::Particles
        ::PARTICLE_PEA_HIT_1, ResourceKeys::Particles::PARTICLE_PEA_HIT_2, 
    ResourceKeys::Particles::PARTICLE_PEA_HIT_3};  // 子弹击中纹理

    config.useTexture = true;
    configs[ParticleType::PEA_BULLET_HIT] = config;

    config.startColor = { 255, 255, 255, 255 }; 
    config.endColor = { 255, 255, 255, 255 };
    config.lifetime = 0.8f;
    config.startSize = 0.75f;
    config.endSize = 0.75f;
    config.minVelocity = 60.0f;
    config.maxVelocity = 110.0f;
    config.spreadAngle = 100.0f;
    config.gravity = 120.0f;
    config.textureKeys =
    { ResourceKeys::Particles::PARTICLE_ZOMBIE_HEAD };
    config.useTexture = true;
    configs[ParticleType::ZOMBIE_HEAD_OFF] = config;

    config.startColor = { 255, 255, 255, 255 };
    config.endColor = { 255, 255, 255, 255 };
    config.lifetime = 0.8f;
    config.startSize = 0.7f;
    config.endSize = 0.7f;
    config.minVelocity = 0.0f;
    config.maxVelocity = 0.0f;
    config.spreadAngle = 100.0f;
    config.gravity = 110.0f;
    config.textureKeys =
    { ResourceKeys::Particles::PARTICLE_ZOMBIEARM };
    config.useTexture = true;
    configs[ParticleType::ZOMBIE_ARM_OFF] = config;

}

const ParticleConfig& ParticleConfigManager::GetConfig(ParticleType effect) const {
    auto it = configs.find(effect);
    if (it != configs.end()) {
        return it->second;
    }

    static ParticleConfig defaultConfig;
    return defaultConfig;
}

SDL_Texture* ParticleConfigManager::GetRandomTextureForEffect(ParticleType effect) const {
    if (!renderer) return nullptr;
    const ParticleConfig& config = GetConfig(effect);
    if (config.useTexture) {
        ResourceManager& resourceManager = ResourceManager::GetInstance();
        std::string textureKey = config.GetRandomTextureKey();

        if (!textureKey.empty()) {
            return resourceManager.GetTexture(textureKey);
        }
    }

    return nullptr;
}

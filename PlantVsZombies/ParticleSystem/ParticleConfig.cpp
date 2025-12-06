#include "../ParticleSystem/ParticleConfig.h"
#include "../ResourceManager.h"
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
    config.startColor = { 255, 255, 255, 255 };  // 使用纹理颜色，所以设为白色
    config.endColor = { 255, 255, 255, 0 };
    config.lifetime = 30.0f;
    config.startSize = 52.0f;
    config.endSize = 10.0f;
    config.minVelocity = 1.0f;
    config.maxVelocity = 3.0f;
    config.spreadAngle = 90.0f;
    config.gravity = 0.2f;
    config.textureKeys = 
    { "particle_zombiehead", "particle_zombiehead", "particle_zombiehead" };  // 子弹击中纹理
    config.useTexture = true;
    configs[ParticleType::PEA_BULLET_HIT] = config;

    config.startColor = { 255, 255, 255, 255 }; 
    config.endColor = { 255, 255, 255, 255 };
    config.lifetime = 2.0f;
    config.startSize = 65.0f;
    config.endSize = 65.0f;
    config.minVelocity = 60.0f;
    config.maxVelocity = 110.0f;
    config.spreadAngle = 80.0f;
    config.gravity = 330.0f; 
    config.textureKeys =
    { "particle_zombiehead" };
    config.useTexture = true;
    configs[ParticleType::ZOMBIE_HEAD_OFF] = config;

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

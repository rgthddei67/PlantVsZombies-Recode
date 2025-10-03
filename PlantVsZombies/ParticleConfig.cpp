#include "ParticleConfig.h"
#include "ResourceManager.h"
#include <cmath>

ParticleConfigManager::ParticleConfigManager(SDL_Renderer* sdlRenderer)
    : renderer(sdlRenderer) {
    InitializeConfigs();
}

void ParticleConfigManager::InitializeConfigs() {
    ResourceManager& resourceManager = ResourceManager::GetInstance();
    // 子弹击中
    ParticleConfig bullet;
    bullet.startColor = { 255, 255, 255, 255 };  // 使用纹理颜色，所以设为白色
    bullet.endColor = { 255, 255, 255, 0 };
    bullet.lifetime = 30;  
    bullet.startSize = 52.0f;
    bullet.endSize = 10.0f;
    bullet.minVelocity = 1.0f;
    bullet.maxVelocity = 3.0f;
    bullet.spreadAngle = 90.0f;  
    bullet.gravity = 0.2f;
    bullet.textureKey = "particle_1";  // 子弹击中纹理
    bullet.useTexture = true;
    configs[ParticleEffect::PEA_BULLET_HIT] = bullet;

}

const ParticleConfig& ParticleConfigManager::GetConfig(ParticleEffect effect) const {
    auto it = configs.find(effect);
    if (it != configs.end()) {
        return it->second;
    }

    static ParticleConfig defaultConfig;
    return defaultConfig;
}

SDL_Texture* ParticleConfigManager::GetTextureForEffect(ParticleEffect effect) const {
    if (!renderer) return nullptr;

    const ParticleConfig& config = GetConfig(effect);
    if (config.useTexture && !config.textureKey.empty()) {
        ResourceManager& resourceManager = ResourceManager::GetInstance();
        return resourceManager.GetTexture(config.textureKey);
    }

    return nullptr;
}
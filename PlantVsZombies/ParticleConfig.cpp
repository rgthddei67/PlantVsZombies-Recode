#include "ParticleConfig.h"
#include "ResourceManager.h"
#include <cmath>

ParticleConfigManager::ParticleConfigManager(SDL_Renderer* sdlRenderer)
    : renderer(sdlRenderer) {
    InitializeConfigs();
}

void ParticleConfigManager::InitializeConfigs() 
{
    ResourceManager& resourceManager = ResourceManager::GetInstance();
    // �ӵ�����
    ParticleConfig config;
    config.startColor = { 255, 255, 255, 255 };  // ʹ��������ɫ��������Ϊ��ɫ
    config.endColor = { 255, 255, 255, 0 };
    config.lifetime = 30;
    config.startSize = 52.0f;
    config.endSize = 10.0f;
    config.minVelocity = 1.0f;
    config.maxVelocity = 3.0f;
    config.spreadAngle = 90.0f;
    config.gravity = 0.2f;
    config.textureKey = "particle_1";  // �ӵ���������
    config.useTexture = true;
    configs[ParticleEffect::PEA_BULLET_HIT] = config;

    config.startColor = { 255, 255, 255, 255 }; 
    config.endColor = { 255, 255, 255, 255 };
    config.lifetime = 120;
    config.startSize = 65.0f;
    config.endSize = 65.0f;
    config.minVelocity = 1.0f;
    config.maxVelocity = 2.0f;
    config.spreadAngle = 70.0f;
    config.gravity = 0.25f;
    config.textureKey = "particle_zombiehead";
    config.useTexture = true;
    configs[ParticleEffect::ZOMBIE_HEAD_OFF] = config;

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
#pragma once
#ifndef __PARTICLE_CONFIG_H__
#define __PARTICLE_CONFIG_H__

#include "Particle.h"
#include "../GameRandom.h"
#include "../ResourceManager.h"
#include <string>
#include <unordered_map>
#include <vector>

struct ParticleConfig {
    glm::vec4 startColor = glm::vec4(1.0f);
    glm::vec4 endColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    float lifetime = 60.0f;
    float startSize = 10.0f;
    float endSize = 5.0f;
    float minVelocity = 50.0f;
    float maxVelocity = 150.0f;
    float spreadAngle = 360.0f; // 旋转角度
    float gravity = 0.5f;
    bool fadeOut = true;
    std::vector<std::string> textureKeys; 

    std::string GetRandomTextureKey() const {
        if (textureKeys.empty()) return "";
        if (textureKeys.size() == 1) return textureKeys[0];
        int index = GameRandom::Range(0, static_cast<int>(textureKeys.size()) - 1);
        return textureKeys[index];
    }
};

class ParticleConfigManager {
private:
    std::unordered_map<ParticleType, ParticleConfig> configs;
    Graphics* m_graphics; 

public:
    ParticleConfigManager(Graphics* graphics);
    ~ParticleConfigManager() = default;

    void SetGraphics(Graphics* graphics) { m_graphics = graphics; }

    const ParticleConfig& GetConfig(ParticleType effect) const;
    const GLTexture* GetRandomTextureForEffect(ParticleType effect) const;  

private:
    void InitializeConfigs();
};

#endif
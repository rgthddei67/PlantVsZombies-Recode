#pragma once
#ifndef __PARTICLE_CONFIG_H__
#define __PARTICLE_CONFIG_H__

#include "ParticleXMLConfig.h"
#include "ParticleXMLLoader.h"
#include "../Graphics.h"
#include <string>
#include <vector>
#include <memory>

class ParticleConfigManager {
private:
    Graphics* m_graphics;

    // XML配置支持
    std::unique_ptr<ParticleXMLLoader> xmlLoader;

public:
    ParticleConfigManager(Graphics* graphics);
    ~ParticleConfigManager() = default;

    void SetGraphics(Graphics* graphics) { m_graphics = graphics; }

    // XML配置支持
    bool LoadXMLConfigs(const std::string& directory);
    const ParticleEffectConfig* GetEffectConfig(const std::string& name) const;
    std::vector<std::string> GetAllXMLEffectNames() const;
};

#endif
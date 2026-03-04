#include "ParticleConfig.h"
#include <iostream>

ParticleConfigManager::ParticleConfigManager(Graphics* graphics)
    : m_graphics(graphics)
    , xmlLoader(std::make_unique<ParticleXMLLoader>()) {
}

bool ParticleConfigManager::LoadXMLConfigs(const std::string& directory) {
    if (!xmlLoader) {
        std::cerr << "错误: XML加载器未初始化" << std::endl;
        return false;
    }
    return xmlLoader->LoadFromDirectory(directory);
}

const ParticleEffectConfig* ParticleConfigManager::GetEffectConfig(const std::string& name) const {
    if (!xmlLoader) {
        return nullptr;
    }
    return xmlLoader->GetEffectConfig(name);
}

std::vector<std::string> ParticleConfigManager::GetAllXMLEffectNames() const {
    if (!xmlLoader) {
        return std::vector<std::string>();
    }
    return xmlLoader->GetAllEffectNames();
}
#include "ParticleSystem.h"
#include <iostream>

std::unique_ptr<ParticleSystem> g_particleSystem = nullptr;

ParticleSystem::ParticleSystem(Graphics* graphics)
    : m_graphics(graphics)
    , configManager(graphics) {
}

ParticleSystem::~ParticleSystem() {
    ClearAll();
}

void ParticleSystem::UpdateAll() {
    CleanupInactiveEmitters();
    CleanupInactiveEffects();

    for (size_t i = 0; i < emitters.size(); i++)
    {
        emitters[i].get()->Update();
    }

    for (size_t i = 0; i < effects.size(); i++)
    {
        effects[i].get()->Update();
    }
}

void ParticleSystem::DrawAll() {
    for (size_t i = 0; i < emitters.size(); i++)
    {
        emitters[i].get()->Draw();
    }

    for (size_t i = 0; i < effects.size(); i++)
    {
        effects[i].get()->Draw();
    }
}

void ParticleSystem::ClearAll() {
    emitters.clear();
    effects.clear();
}

void ParticleSystem::CleanupInactiveEmitters() {
    emitters.erase(
        std::remove_if(emitters.begin(), emitters.end(),
            [](const std::unique_ptr<ParticleEmitter>& emitter) {
                return emitter->ShouldDestroy();
            }),
        emitters.end()
    );
}

int ParticleSystem::GetTotalParticles() const {
    int total = 0;
    for (const auto& emitter : emitters) {
        total += emitter->GetActiveParticleCount();
    }
    return total;
}

size_t ParticleSystem::GetActiveEmitters() const {
    return emitters.size();
}

bool ParticleSystem::LoadXMLConfigs(const std::string& directory) {
    return configManager.LoadXMLConfigs(directory);
}

void ParticleSystem::EmitEffect(const std::string& effectName, const Vector& position) {
    const ParticleEffectConfig* config = configManager.GetEffectConfig(effectName);
    if (!config) {
        std::cerr << "错误: 找不到粒子特效配置: " << effectName << std::endl;
        return;
    }

    auto effect = std::make_unique<ParticleEffect>();
    effect->InitializeFromConfig(*config, m_graphics, position);
    effects.push_back(std::move(effect));
}

void ParticleSystem::CleanupInactiveEffects() {
    effects.erase(
        std::remove_if(effects.begin(), effects.end(),
            [](const std::unique_ptr<ParticleEffect>& effect) {
                return effect->ShouldDestroy();
            }),
        effects.end()
    );
}
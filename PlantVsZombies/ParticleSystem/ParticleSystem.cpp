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
	CleanupInactiveEffects();

	for (auto& effect : effects) {
		effect->Update();
	}
}

void ParticleSystem::DrawAll() {
	for (auto& effect : effects) {
		effect->Draw();
	}
}

void ParticleSystem::ClearAll() {
	effects.clear();
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
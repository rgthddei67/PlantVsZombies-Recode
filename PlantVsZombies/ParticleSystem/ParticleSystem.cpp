#include "ParticleSystem.h"
#include "../Logger.h"

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

void ParticleSystem::DrawBelow(int order) {
	for (auto& effect : effects) {
		if (effect->GetRenderOrder() < order) {
			effect->Draw();
		}
	}
}

void ParticleSystem::DrawFrom(int order) {
	for (auto& effect : effects) {
		if (effect->GetRenderOrder() >= order) {
			effect->Draw();
		}
	}
}

void ParticleSystem::ClearAll() {
	effects.clear();
}

bool ParticleSystem::LoadXMLConfigs(const std::string& directory) {
	return configManager.LoadXMLConfigs(directory);
}

void ParticleSystem::EmitEffect(const std::string& effectName, const Vector& position,
	int renderOrder, float durationOverride, float clipRightX) {
	const ParticleEffectConfig* config = configManager.GetEffectConfig(effectName);
	if (!config) {
		LOG_ERROR("Particle") << "找不到粒子特效配置: " << effectName;
		return;
	}

	auto effect = std::make_unique<ParticleEffect>();
	effect->InitializeFromConfig(*config, m_graphics, position);
	effect->SetName(effectName);
	effect->SetRenderOrder(renderOrder);
	effect->SetSystemDuration(durationOverride);
	effect->SetClipRightX(clipRightX);
	effects.push_back(std::move(effect));
}

void ParticleSystem::StopEffect(const std::string& effectName) {
	for (auto& effect : effects) {
		if (effect->GetName() == effectName) effect->Stop();
	}
}

bool ParticleSystem::IsEffectEmitting(const std::string& effectName) const {
	for (const auto& effect : effects) {
		if (effect->GetName() == effectName && effect->IsEmitting()) return true;
	}
	return false;
}

int ParticleSystem::GetEffectActiveParticleCount(const std::string& effectName) const {
	int count = 0;
	for (const auto& effect : effects) {
		if (effect->GetName() == effectName) count += effect->GetActiveParticleCount();
	}
	return count;
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

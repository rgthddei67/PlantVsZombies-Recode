#pragma once
#ifndef __PARTICLE_SYSTEM_H__
#define __PARTICLE_SYSTEM_H__

#include "ParticleEffect.h"
#include "ParticleConfig.h"
#include "../Game/Definit.h"
#include "../Graphics.h"
#include <vector>
#include <memory>

class ParticleSystem {
private:
	std::vector<std::unique_ptr<ParticleEffect>> effects;
	Graphics* m_graphics;
	ParticleConfigManager configManager;

public:
	explicit ParticleSystem(Graphics* graphics);
	~ParticleSystem();

	ParticleSystem(const ParticleSystem&) = delete;
	ParticleSystem& operator=(const ParticleSystem&) = delete;

	void UpdateAll();
	void DrawAll();
	void ClearAll();

	bool LoadXMLConfigs(const std::string& directory);
	void EmitEffect(const std::string& effectName, const Vector& position);

private:
	void CleanupInactiveEffects();
};

extern std::unique_ptr<ParticleSystem> g_particleSystem;

#endif
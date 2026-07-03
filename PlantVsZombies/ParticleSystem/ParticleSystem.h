#pragma once
#ifndef __PARTICLE_SYSTEM_H__
#define __PARTICLE_SYSTEM_H__

#include "ParticleEffect.h"
#include "ParticleConfig.h"
#include "../Game/Definit.h"
#include "../Game/RenderOrder.h"
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
	// 绘制 renderOrder < order 的特效（战场层；由 GameObjectManager overlay 前 hook 调用）
	void DrawBelow(int order);
	// 绘制 renderOrder >= order 的特效（顶层；由场景命令槽调用）
	void DrawFrom(int order);
	void ClearAll();

	bool LoadXMLConfigs(const std::string& directory);
	void EmitEffect(const std::string& effectName, const Vector& position,
		int renderOrder = LAYER_EFFECTS_WORLD);

private:
	void CleanupInactiveEffects();
};

extern std::unique_ptr<ParticleSystem> g_particleSystem;

#endif
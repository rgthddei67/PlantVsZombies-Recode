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
	Graphics* m_graphics = nullptr;
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
		int renderOrder = LAYER_EFFECTS_WORLD, float durationOverride = -1.0f);
	/** 停止全部同名特效继续发射；已生成粒子仍按各自寿命自然收尾。 */
	void StopEffect(const std::string& effectName);
	/** 查询同名特效是否至少有一个发射器仍在工作。 */
	bool IsEffectEmitting(const std::string& effectName) const;
	/** 返回全部同名特效当前存活粒子总数，供 AutoTest 和问题诊断使用。 */
	int GetEffectActiveParticleCount(const std::string& effectName) const;

private:
	void CleanupInactiveEffects();
};

extern std::unique_ptr<ParticleSystem> g_particleSystem;

#endif

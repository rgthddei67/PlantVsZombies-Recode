#pragma once
#ifndef __PARTICLE_EFFECT_H__
#define __PARTICLE_EFFECT_H__

#include "ParticleEmitter.h"
#include "ParticleXMLConfig.h"
#include "../Game/Definit.h"
#include <vector>
#include <memory>

// 多发射器粒子特效管理器
class ParticleEffect {
private:
	std::vector<std::unique_ptr<ParticleEmitter>> emitters;
	Vector position;
	float systemTimer;
	float systemDuration;
	bool active;
	int renderOrder = 0;

public:
	ParticleEffect();
	~ParticleEffect() = default;

	// 从XML配置初始化
	void InitializeFromConfig(const ParticleEffectConfig& config, Graphics* graphics, const Vector& pos);

	void Update();
	void Draw();

	bool IsActive() const { return active; }
	bool ShouldDestroy() const;

	void SetRenderOrder(int order) { renderOrder = order; }
	int GetRenderOrder() const { return renderOrder; }

	void SetPosition(const Vector& pos);
	Vector GetPosition() const { return position; }
};

#endif
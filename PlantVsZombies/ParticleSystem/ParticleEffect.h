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
	float systemTimer = 0.0f;
	float systemDuration = -1.0f;
	bool active = false;
	int renderOrder = 0;

public:
	ParticleEffect() = default;
	~ParticleEffect() = default;

	// 从XML配置初始化
	void InitializeFromConfig(const ParticleEffectConfig& config, Graphics* graphics, const Vector& pos);

	void Update();
	void Draw();

	bool IsActive() const { return active; }
	bool ShouldDestroy() const;

	void SetRenderOrder(int order) { renderOrder = order; }
	int GetRenderOrder() const { return renderOrder; }
	// 天气等持续特效可用运行期时长覆盖 XML 默认值；<=0 保留配置值。
	void SetSystemDuration(float duration) { if (duration > 0.0f) systemDuration = duration; }

	void SetPosition(const Vector& pos);
	Vector GetPosition() const { return position; }
};

#endif

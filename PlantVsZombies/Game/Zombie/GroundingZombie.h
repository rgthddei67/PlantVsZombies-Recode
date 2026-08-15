#pragma once

#include "ConeZombie.h"

/**
 * 第六大关接地僵尸：1200 生命的天线路障为满电推演提供植物专属引导候选。
 * 普通移动、啃食、三阶段破损、断肢断头与死亡帧事件全部复用路障僵尸。
 */
class GroundingZombie final : public ConeZombie {
public:
	using ConeZombie::ConeZombie;

	bool IsNightRoofChargeGuideType() const override { return true; }
	bool CanGuideNightRoofCharge() const override;
	bool TryGetNightRoofChargeGuideAnchor(Vector& anchor) const override;

protected:
	void SetupZombie() override;
	const std::string& GetConeTextureKey(ArmorBrokenState stage) const override;
	const char* GetConeDropEffectName() const override {
		return "GroundingZombieConeOff";
	}
};

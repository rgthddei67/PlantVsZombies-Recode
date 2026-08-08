#pragma once

#include "CatapultZombie.h"

/**
 * @brief 导流投篮车僵尸：自然锁行时引导自身所在行，并独享更强顺坡漂移。
 */
class EliteCatapultZombie final : public CatapultZombie {
public:
	using CatapultZombie::CatapultZombie;

	bool CanGuideRoofRunoff() const override;
	float GetRoofRunoffDriftMultiplier() const override;

protected:
	void SetupZombie() override;
	const std::string& GetCatapultSidingTextureKey(bool damaged) const override;
	const char* GetCatapultExplosionEffectName() const override;
};

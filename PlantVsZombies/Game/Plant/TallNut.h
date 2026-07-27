#pragma once

#include "WallNut.h"

/**
 * @brief 经典高坚果：拥有双倍坚果生命，并阻拦撑杆与海豚两类跳跃。
 */
class TallNut final : public WallNut {
public:
	using WallNut::WallNut;

	bool BlocksZombieJump(ZombieJumpType jumpType) const override;
	void OnZombieJumpBlocked(ZombieJumpType jumpType) override;

protected:
	void SetupPlant() override;
	const std::string& GetBodyTextureKey() const override;
	const std::string& GetCrackedTextureKey(int damageStage) const override;
	const char* GetDamageTrackName() const override { return "anim_idle"; }
	float GetCrackParticleOffsetY() const override { return -72.0f; }
};

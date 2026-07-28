#pragma once

#include "DolphinRiderZombie.h"

/**
 * @brief 精英海豚骑士：第一次成功越障后保留海豚，第二次才弃豚，被挡时撞伤植物。
 */
class EliteDolphinRiderZombie final : public DolphinRiderZombie {
public:
	using DolphinRiderZombie::DolphinRiderZombie;

	int GetDolphinJumpCapacity() const override;

protected:
	void SetupZombie() override;
	void OnDolphinJumpBlocked(Plant& blockingPlant) override;
	const std::string& GetLostOuterArmTextureKey() const override;
	const char* GetDolphinHeadOffEffectName() const override;
};

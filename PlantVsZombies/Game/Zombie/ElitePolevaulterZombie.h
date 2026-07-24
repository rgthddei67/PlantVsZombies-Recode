#pragma once

#include "Polevaulter.h"

/**
 * @brief 绿色精英撑杆僵尸：双倍跳距，并在最终落点生成普通撑杆僵尸。
 */
class ElitePolevaulterZombie : public Polevaulter {
public:
	using Polevaulter::Polevaulter;

	void HeadDrop() override;

protected:
	void SetupZombie() override;
	float GetVaultDistance() const override;
	float GetAbilityAnimSpeedMultiplier() const override;
	void OnVaultLanded() override;
};

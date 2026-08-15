#pragma once

#include "MagnetShroom.h"

/**
 * @brief 磁暴菇：成功剥离僵尸铁器后，以目标为中心释放无伤害麻痹脉冲。
 */
class GoldMagnet final : public MagnetShroom {
public:
	using MagnetShroom::MagnetShroom;

	float GetSimulationAbilityCooldownRemaining() const override;

protected:
	float GetRechargeSeconds() const override;
	const char* GetSleepTrackName() const override { return "anim_idle"; }
	const char* GetShootingTrackName() const override { return "anim_attract"; }
	const char* GetChargingTrackName() const override { return "anim_idle"; }
	void OnZombieMagneticItemExtracted(
		const MagneticItem& item, const Vector& targetCenter, int targetRow) override;
};

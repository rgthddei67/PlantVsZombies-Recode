#pragma once

#include "MagnetShroom.h"

/**
 * @brief 磁暴菇：成功剥离僵尸铁器后，以目标为中心释放无伤害麻痹脉冲。
 */
class GoldMagnet final : public MagnetShroom {
public:
	using MagnetShroom::MagnetShroom;

	/** 原版金磁力菇不是夜间植物；任何外部睡眠请求都收敛为保持清醒。 */
	void SetSleepState(bool sleep) override;
	/** 旧存档或升级继承的睡眠状态不得让金磁力菇重新进入咖啡豆流程。 */
	void RestoreSleepState(bool sleep, float wakeUpTimeRemaining) override;
	float GetSimulationAbilityCooldownRemaining() const override;

protected:
	float GetRechargeSeconds() const override;
	const char* GetShootingTrackName() const override { return "anim_attract"; }
	const char* GetChargingTrackName() const override { return "anim_idle"; }
	void OnZombieMagneticItemExtracted(
		const MagneticItem& item, const Vector& targetCenter, int targetRow) override;
};

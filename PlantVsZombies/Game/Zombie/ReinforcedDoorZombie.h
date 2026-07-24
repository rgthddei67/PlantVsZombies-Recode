#pragma once

#include "DoorZombie.h"

/**
 * 加固铁门僵尸：持门时限制植物单次伤害并阻断大喷穿透，灰烬与植物直杀走专属耐性。
 */
class ReinforcedDoorZombie final : public DoorZombie {
protected:
	void SetupZombie() override;
	const char* GetDoorImageKey(ArmorBrokenState stage) const override;
	const char* GetDoorDropParticleName() const override { return "ZombieReinforcedDoorOff"; }
	int AdjustIncomingDamage(int damage, DamageSource source, bool penetrateShield) const override;

public:
	using DoorZombie::DoorZombie;

	bool CanBeCharred() const override;
	bool CanBeCharmed() const override { return false; }
	bool ResistsTangleKelpDrowning() const override {
		return mShieldType != ShieldType::SHIELDTYPE_NONE;
	}
	bool BlocksFumePiercing() const override {
		return mShieldType != ShieldType::SHIELDTYPE_NONE;
	}
	int ModifyFumeDamage(int damage) const override;
	bool TakePlantInstantKill() override;
};

#include "ReinforcedDoorZombie.h"
#include <algorithm>

namespace {
	constexpr int kBodyHealth = 500;                    // 本体生命值
	constexpr int kShieldHealth = 1100;                 // 加固铁门生命值
	constexpr int kShieldedPlantDamageCap = 10;         // 持门时植物普通伤害的最终单次上限
	constexpr int kAshDamageCap = 100;                  // 灰烬/爆炸伤害的最终单次上限
	constexpr int kFumeDamageMultiplier = 2;            // 大喷菇与寒冰大喷菇基础伤害倍率
	constexpr int kPlantInstantKillFallbackDamage = 20; // 大嘴花等植物直杀失败后结算的普通伤害
}

void ReinforcedDoorZombie::SetupZombie()
{
	// 完整复用铁门僵尸的帧事件、走路、门臂和死亡时序，仅覆盖数值与材质。
	DoorZombie::SetupZombie();
	mBodyHealth = kBodyHealth;
	mBodyMaxHealth = kBodyHealth;
	if (!mIsPreview) {
		mShieldHealth = kShieldHealth;
		mShieldMaxHealth = kShieldHealth;
	}
	ApplyDoorImage();
}

const char* ReinforcedDoorZombie::GetDoorImageKey(ArmorBrokenState stage) const
{
	switch (stage) {
	case ArmorBrokenState::NO_BROKEN:
		return "IMAGE_ZOMBIE_REINFORCED_SCREENDOOR1";
	case ArmorBrokenState::A_LITTLE_BROKEN:
		return "IMAGE_ZOMBIE_REINFORCED_SCREENDOOR2";
	case ArmorBrokenState::REALLY_BROKEN:
		return "IMAGE_ZOMBIE_REINFORCED_SCREENDOOR3";
	default:
		return nullptr;
	}
}

int ReinforcedDoorZombie::AdjustIncomingDamage(
	int damage, DamageSource source, bool /*penetrateShield*/) const
{
	// 上限作用于词条缩放后的最终单次伤害，确保增伤词条不能越过设计阈值。
	if (source == DamageSource::PLANT_ASH) {
		return std::min(damage, kAshDamageCap);
	}
	if (source == DamageSource::PLANT && mShieldType != ShieldType::SHIELDTYPE_NONE) {
		return std::min(damage, kShieldedPlantDamageCap);
	}
	return damage;
}

int ReinforcedDoorZombie::ModifyFumeDamage(int damage) const
{
	return damage * kFumeDamageMultiplier;
}

void ReinforcedDoorZombie::TakePlantInstantKill()
{
	// 直杀不能绕过耐久；持门时仍由最终伤害钩子确保不超过 20。
	TakeDamage(kPlantInstantKillFallbackDamage, DamageSource::PLANT);
}

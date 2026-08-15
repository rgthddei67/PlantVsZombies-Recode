#include "ReinforcedDoorZombie.h"
#include <algorithm>

namespace {
	constexpr int kBodyHealth = 270;                    // 本体生命值
	constexpr int kShieldHealth = 1030;                 // 加固铁门生命值
	constexpr int kShieldedNormalDamageCap = 10;         // 持门时植物普通伤害的最终单次上限
	constexpr int kAshDamageCap = 320;                  // 灰烬/爆炸伤害的最终单次上限
	constexpr int kFumeDamageMultiplier = 2;            // 大喷菇与寒冰大喷菇基础伤害倍率
	constexpr int kShieldedSpikeFrameDamageCap = 1;     // 持门时仙人掌尖刺每个 1x 碰撞帧的基础伤害上限
	constexpr int kPlantInstantKillFallbackDamage = 10; // 持门拒吞时保留的特殊基础伤害
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
	int damage, DamageSource source, bool /*penetrateShield*/, bool bypassShield) const
{
	// 上限作用于词条缩放后的最终单次伤害，确保增伤词条不能越过设计阈值。
	if (source == DamageSource::PLANT_ASH && mShieldType != ShieldType::SHIELDTYPE_NONE) 
	{
		return std::min(damage, kAshDamageCap);
	}
	if (source == DamageSource::PLANT && mShieldType != ShieldType::SHIELDTYPE_NONE
		&& !bypassShield) {
		return std::min(damage, kShieldedNormalDamageCap);
	}
	return damage;
}

int ReinforcedDoorZombie::ModifyFumeDamage(int damage) const
{
	return damage * kFumeDamageMultiplier;
}

float ReinforcedDoorZombie::ModifySpikeFrameDamage(float damage, bool bypassShield) const
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE || bypassShield) {
		return damage;
	}
	return std::min(damage, static_cast<float>(kShieldedSpikeFrameDamageCap));
}

bool ReinforcedDoorZombie::TakePlantInstantKill()
{
	// 破门后恢复普通吞食；持门时只拒绝吞食，攻击者再走正式伤害链结算咬伤。
	if (mShieldType == ShieldType::SHIELDTYPE_NONE)
	{
		this->Die();
		return true;
	}
	return false;
}

int ReinforcedDoorZombie::AdjustRejectedChomperBiteDamage(int /*damage*/) const
{
	return kPlantInstantKillFallbackDamage;
}

bool ReinforcedDoorZombie::CanBeCharred() const
{
	if (this->mShieldType != ShieldType::SHIELDTYPE_NONE)
	{
		return false;
	}
	else 
	{
		return true;
	}
}

#include "GroundingShroom.h"

#include "../Cell.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr int kGroundingShroomHealth = 500;          // 接地菇作为常驻雷荷防护植物的基础生命值
	constexpr int kDryBacklashDamage = 100;             // 普通瓦面每次导电直接扣除的本体生命
	constexpr int kWetBacklashDamage = 150;             // 自身所在列为湿坡时每次导电直接扣除的本体生命
	constexpr int kProtectedColumnRadius = 1;            // 同排保护自身列与左右各一列
	constexpr float kZombieGroundingRadiusCells = 1.5f; // 僵尸按三格条带的连续水平边界判断接地
	constexpr float kShadowScale = 0.72f;                // 脚底影子相对默认贴图的缩放
	constexpr float kShadowOffsetY = 32.0f;              // 影子相对格中心的垂直偏移，单位 px
	constexpr float kShockClipSpeed = 1.0f;              // 受电一次性轨相对 reanim 基础帧率的倍率
}

void GroundingShroom::SetupPlant()
{
	Shroom::SetupPlant();
	mPlantHealth = kGroundingShroomHealth;
	mPlantMaxHealth = kGroundingShroomHealth;
	if (auto* shadow = GetShadow()) {
		shadow->SetScale(Vector(kShadowScale, kShadowScale));
		shadow->SetOffset(Vector(0.0f, kShadowOffsetY));
	}
	if (!mIsSleeping) PlayTrack("anim_idle");
}

bool GroundingShroom::CanGroundNightRoofChargeFor(const Plant* target) const
{
	return target && IsActive() && !mIsPreview && !mIsSleeping
		&& !mIsSquished && !IsBungeeTargeted()
		&& target->mRow == mRow
		&& std::abs(target->mColumn - mColumn) <= kProtectedColumnRadius;
}

bool GroundingShroom::SuppressesNightRoofChargeProtectionFor(
	const Zombie* target) const
{
	if (!target || !IsActive() || mIsPreview || mIsSleeping
		|| mIsSquished || IsBungeeTargeted() || target->mRow != mRow) {
		return false;
	}
	return std::abs(target->GetPosition().x - GetPosition().x)
		<= kZombieGroundingRadiusCells * CELL_COLLIDER_SIZE_X;
}

void GroundingShroom::AbsorbGroundedNightRoofCharge(bool onWetSlope)
{
	if (!IsActive()) return;
	// 所有目标已经由 Board 冻结分配；这里直接扣本体，不能借南瓜或词条转移反噬。
	mPlantHealth = std::max(0, mPlantHealth
		- (onWetSlope ? kWetBacklashDamage : kDryBacklashDamage));
	SetGlowingTimer(0.1f);
	PlayTrackOnce("anim_shooting", "anim_idle", kShockClipSpeed,
		0.0f, 0.0f, 0.0f);
	if (mPlantHealth <= 0) Die();
}

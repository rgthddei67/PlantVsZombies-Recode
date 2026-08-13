#include "LightningRodPot.h"

#include "../Board.h"

namespace {
	constexpr int kLightningRodPotHealth = 700;            // 避雷花盆的基础生命值，仅承受外部正常伤害
	constexpr float kLightningRodPotDamageMultiplier = 2.0f; // 同排普通雷击伤害倍率，多盆由 Board 取最大值
	constexpr float kDischargeAnimationSpeed = 1.0f;       // 避雷针受电一次性轨相对 reanim 基础帧率的倍率
}

void LightningRodPot::SetupPlant()
{
	FlowerPot::SetupPlant();
	mPlantHealth = kLightningRodPotHealth;
	mPlantMaxHealth = kLightningRodPotHealth;
	PlayTrack("anim_idle");
}

bool LightningRodPot::HasActiveSupportedHost() const
{
	if (!mBoard || !IsActive() || mIsPreview || mPlantHealth <= 0
		|| IsSquished() || IsBungeeTargeted()) {
		return false;
	}
	for (Plant* host : {
		mBoard->GetNormalPlantAt(mRow, mColumn),
		mBoard->GetPumpkinAt(mRow, mColumn) }) {
		if (host && host->IsActive() && !host->IsPreview()
			&& host->mPlantHealth > 0 && !host->IsSquished()) {
			return true;
		}
	}
	return false;
}

bool LightningRodPot::ProtectsSupportedLayer(const Plant* target) const
{
	if (!target || !HasActiveSupportedHost()
		|| target->mRow != mRow || target->mColumn != mColumn
		|| !target->IsActive() || target->IsPreview() || target->IsSquished()) {
		return false;
	}
	return target == mBoard->GetNormalPlantAt(mRow, mColumn)
		|| target == mBoard->GetPumpkinAt(mRow, mColumn)
		|| target == mBoard->GetOverlayPlantAt(mRow, mColumn);
}

bool LightningRodPot::ProtectsSupportedPlantFromNightRoofCharge(
	const Plant* target) const
{
	return ProtectsSupportedLayer(target);
}

bool LightningRodPot::ProtectsSupportedPlantFromNightRoofHijacker(
	const Plant* target) const
{
	return ProtectsSupportedLayer(target);
}

float LightningRodPot::GetNightRoofChargeZombieDamageMultiplier() const
{
	return HasActiveSupportedHost() ? kLightningRodPotDamageMultiplier : 1.0f;
}

void LightningRodPot::OnNightRoofChargeProtectionTriggered()
{
	if (!IsActive()) return;
	SetGlowingTimer(0.18f);
	PlayTrackOnce("anim_discharge", "anim_idle", kDischargeAnimationSpeed,
		0.0f, 0.0f, 0.0f);
}

#pragma once

#include "FlowerPot.h"

/**
 * 避雷花盆：原位升级花盆，在黑夜屋顶放电边沿保护同格上层并强化同排雷击。
 */
class LightningRodPot final : public FlowerPot {
public:
	using FlowerPot::FlowerPot;

	bool ProtectsSupportedPlantFromNightRoofCharge(const Plant* target) const override;
	bool ProtectsSupportedPlantFromNightRoofHijacker(const Plant* target) const override;
	float GetNightRoofChargeZombieDamageMultiplier() const override;
	void OnNightRoofChargeProtectionTriggered() override;

protected:
	void SetupPlant() override;
	bool PausesAnimationWhenCovered() const override { return false; }

private:
	/** 返回自身格是否属于目标 footprint，且该格的上层植物栈仍引用该目标。 */
	bool ProtectsSupportedLayer(const Plant* target) const;
	/** 空盆不提供同排伤害倍率；普通层或南瓜层任一有效即视为正在承载。 */
	bool HasActiveSupportedHost() const;
};

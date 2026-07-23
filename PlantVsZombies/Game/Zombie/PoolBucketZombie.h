#pragma once

#include "PoolNormalZombie.h"

/** 带铁桶头盔的泳池僵尸；游泳行为继承 PoolNormalZombie。 */
class PoolBucketZombie : public PoolNormalZombie {
public:
	using PoolNormalZombie::PoolNormalZombie;

	ArmorBrokenState mHelmStage = ArmorBrokenState::NO_BROKEN;

	void HelmDrop() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	void ZombieItemUpdate() const override;

protected:
	void SetupZombie() override;
	void CheckHelmImage() override;
};

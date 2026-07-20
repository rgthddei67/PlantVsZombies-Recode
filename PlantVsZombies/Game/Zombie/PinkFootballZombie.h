#pragma once

#include "FootballZombie.h"

/**
 * @brief 黑夜专属的粉色橄榄球僵尸：首次啃食重击，头盔破碎时伤害近邻植物。
 */
class PinkFootballZombie : public FootballZombie {
protected:
	void SetupZombie() override;
	void CheckHelmImage() override;

private:
	bool mFirstPlantStrikeUsed = false;

	/** @brief 对以本体为圆心、半径 120 像素内的植物结算头盔破碎伤害。 */
	void DamagePlantsNearBrokenHelmet();

public:
	using FootballZombie::FootballZombie;

	void EatTarget() override;
	void HelmDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;

	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
};

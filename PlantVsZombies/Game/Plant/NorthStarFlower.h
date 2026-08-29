#pragma once

#include "Plant.h"

/**
 * @brief 北极星花：蓄能后按需开启九格极夜导航领域。
 *
 * 领域只在真实受白毛风干扰的索敌或抛射动作发生时消费，覆盖按逻辑格判定。
 */
class NorthStarFlower final : public Plant {
public:
	using Plant::Plant;

	void PlantUpdate() override;
	void Draw(Graphics* g) override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

	bool CoversPolarNavigationCell(int row, int column) const override;
	bool IsPolarNavigationActive() const override {
		return IsActive() && !IsSquished() && mActiveRemaining > 0.0f;
	}
	bool IsPolarNavigationReady() const override;
	bool ActivatePolarNavigation() override;

	float GetChargeSeconds() const { return mChargeSeconds; }
	float GetActiveRemaining() const { return mActiveRemaining; }

protected:
	void SetupPlant() override;

private:
	float mChargeSeconds = 0.0f;
	float mActiveRemaining = 0.0f;
};

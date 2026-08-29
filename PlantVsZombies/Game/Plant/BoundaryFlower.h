#pragma once

#include "Plant.h"

/**
 * 界碑花：在 3x3 领域内积蓄至多两枚碎片，并拒绝敌方非连续入场事务。
 * 主体复用金盏花时间轴，界碑身份由命名 follower 与粒子建立。
 */
class BoundaryFlower final : public Plant {
public:
	using Plant::Plant;

	void PlantUpdate() override;
	bool CoversBoundaryEntryCell(int row, int column) const override;
	bool TryConsumeBoundaryShard() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	int GetShardCount() const { return mShardCount; }
	float GetShardCharge() const { return mShardCharge; }

protected:
	void SetupPlant() override;

private:
	void ConfigureRig();
	void RefreshPresentation() const;

	int mShardCount = 0;
	float mShardCharge = 0.0f;
	bool mRigConfigured = false;
};

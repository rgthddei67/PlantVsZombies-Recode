#pragma once

#include "Plant.h"

/**
 * @brief 经典大蒜：低耐久防御植物，被咬后由僵尸基类执行相邻行改道。
 */
class Garlic final : public Plant {
public:
	using Plant::Plant;

	void PlantUpdate() override;
	void LoadExtraData(const nlohmann::json&) override;
	int GetDamageStage() const { return mDamageStage; }

protected:
	void SetupPlant() override;

private:
	/** 按已保存生命值重建三档身体与最低血量的枯萎茎表现。 */
	void ApplyDamagePresentation();

	int mDamageStage = -1;
};

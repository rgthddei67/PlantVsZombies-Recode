#pragma once
#ifndef _SUNSHROOM_H
#define _SUNSHROOM_H

#include "Shroom.h"

class SunShroom : public Shroom {
protected:
	static constexpr float GROW_TIME    = 50.0f; // 长大时间
	static constexpr float PRODUCE_TIME = 20.0f; // 生产阳光时间

	float mGrowTimer = 0.0f; // 长大计时器
	bool mIsGrown = false; // 是否已经长大

	float mProduceTimer = 15.0f;	// 生产计时器
	bool mIsGlowingForProduction = false;  // 标记是否正在为生产发光
	float mProductionGlowStartTimer = 0.0f;  // 发光开始时间
	bool mGlowProducesGrownSun = false;  // 本轮发光启动时锁定的成熟度：true=产普通阳光(25)，false=产小阳光(15)

public:
	using Shroom::Shroom;

	void PlantUpdate() override;
	void SetupPlant() override;

	void SaveExtraData(nlohmann::json& j) const override {
		j["growTimer"] = mGrowTimer;
		j["isGrown"] = mIsGrown;
		j["produceTimer"] = mProduceTimer;
		j["isGlowingForProduction"] = mIsGlowingForProduction;
		j["productionGlowStartTimer"] = mProductionGlowStartTimer;
		j["glowProducesGrownSun"] = mGlowProducesGrownSun;
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mGrowTimer = j.value("growTimer", 0.0f);
		mIsGrown = j.value("isGrown", false);
		mProduceTimer = j.value("produceTimer", 15.0f);
		mIsGlowingForProduction = j.value("isGlowingForProduction", false);
		mProductionGlowStartTimer = j.value("productionGlowStartTimer", 0.0f);
		mGlowProducesGrownSun = j.value("glowProducesGrownSun", false);
	}
};

#endif

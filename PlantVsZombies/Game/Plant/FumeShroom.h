#pragma once
#ifndef _FUME_SHROOM_H
#define _FUME_SHROOM_H

#include "Shroom.h"

class FumeShroom : public Shroom {
protected:
	float mCheckZombieTimer = 0.0f;
	float mShootTime = 1.5f;     // 射击间隔时间
	float mShootTimer = 1.0f;    // 射击计时器

	bool HasZombieInRow();		// 检测本行是否有僵尸
	void FumeAttack();			// 喷雾区域攻击：对本行锥形范围内全部僵尸瞬时穿透伤害

	void SetupPlant() override;

public:
	using Shroom::Shroom;

	void SaveExtraData(nlohmann::json& j) const override {
		j["shootTimer"] = mShootTimer;
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mShootTimer = j.value("shootTimer", 1.0f);
	}

	void PlantUpdate() override;
};

#endif
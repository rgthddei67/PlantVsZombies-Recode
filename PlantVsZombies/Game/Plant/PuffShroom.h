#pragma once
#ifndef _H_PUFFSHROOM_H
#define _H_PUFFSHROOM_H

#include "Shroom.h"
#include "../Board.h"

class PuffShroom : public Shroom
{
public:
	using Shroom::Shroom;

	void SaveExtraData(nlohmann::json& j) const override {
		j["shootTimer"] = mShootTimer;
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mShootTimer = j.value("shootTimer", 1.0f);
	}

	void PlantUpdate() override;

protected:
	float mCheckZombieTimer = 0.0f;
	float mShootTime = 1.5f;     // 射击间隔时间
	float mShootTimer = 1.0f;    // 射击计时器

	bool HasZombieInRow();		// 检测本行是否有僵尸

	void SetupPlant() override;

};

#endif
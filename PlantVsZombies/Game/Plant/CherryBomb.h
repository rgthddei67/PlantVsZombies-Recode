#pragma once
#ifndef _CHERRYBOMB_H
#define _CHERRYBOMB_H

#include "Plant.h"

class CherryBomb : public Plant {
private:
	float mBombTimer = 0.0f;
	float mBombTime = 1.5f;

public:
	using Plant::Plant;

	void SetupPlant() override;
	void PlantUpdate() override;

	void SaveExtraData(nlohmann::json& j) const override {
		j["bombTimer"] = mBombTimer;
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mBombTimer = j.value("bombTimer", 0.0f);
	}


};

#endif
#pragma once
#ifndef _WALLNUT_H
#define _WALLNUT_H

#include "Plant.h"

class WallNut : public Plant {
private:
	float mUpdateTextureTimer = 0.0f;

public:
	using Plant::Plant;

	void PlantUpdate() override;

	void SaveExtraData(nlohmann::json& j) const override
	{
		j["updateTextureTimer"] = mUpdateTextureTimer;
	}

	void LoadExtraData(const nlohmann::json& j) override
	{
		UpdateTexture();
		mUpdateTextureTimer = j.value("updateTextureTimer", 0.0f);
	}

protected:
	void SetupPlant() override;

	// 植物材质更新
	virtual void UpdateTexture();
};

#endif
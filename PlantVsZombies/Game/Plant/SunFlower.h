#pragma once
#ifndef _SUNFLOWER_H
#define _SUNFLOWER_H

#include "Plant.h"
#include "../Board.h"
#include "../../DeltaTime.h"

class SunFlower : public Plant
{
private:
	float mProduceTimer = 0.0f;
	float mProduceTime = 5.0f;

public:
	using Plant::Plant;		// ¼Ì³Ð¹¹Ôìº¯Êý

	void PlantUpdate() override
	{
		Plant::PlantUpdate();
		mProduceTimer += DeltaTime::GetDeltaTime();
		if (mProduceTimer >= mProduceTime) {
			mProduceTimer = 0.0f;
			mBoard->CreateSun(this->mTransform.lock()->position);
		}
	}

protected:
	void SetupPlant() override
	{
		Plant::SetupPlant();
	}
};

#endif
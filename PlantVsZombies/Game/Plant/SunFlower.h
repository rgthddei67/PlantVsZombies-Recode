#pragma once
#ifndef _SUNFLOWER_H
#define _SUNFLOWER_H

#include "Plant.h"
#include "../Board.h"
#include "../../DeltaTime.h"

class SunFlower : Plant
{
private:
	float mProduceTimer = 0.0f;
	float mProduceTime = 5.0f;

public:
	void Update() override
	{
		mProduceTimer += DeltaTime::GetDeltaTime();
		if (mProduceTimer >= mProduceTime) {
			mProduceTimer = 0.0f;
			mBoard->CreateSun(this->mTransform->position);
		}
	}

protected:
	void SetupPlant() override
	{
		
	}
};

#endif
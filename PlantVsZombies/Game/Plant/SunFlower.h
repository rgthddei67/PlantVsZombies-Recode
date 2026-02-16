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
	bool mIsGlowingForProduction = false;  // 标记是否正在为生产发光
	float mProductionGlowStartTimer = 0.0f;  // 发光开始时间

public:
	using Plant::Plant;		// 继承构造函数

    void PlantUpdate() override
    {
        Plant::PlantUpdate();
        if (!mIsGlowingForProduction) {
            // 正常计时生产
            mProduceTimer += DeltaTime::GetDeltaTime();
            if (mProduceTimer >= mProduceTime) {
                SetGlowingTimer(0.75f);
                mIsGlowingForProduction = true;
                mProductionGlowStartTimer = mProduceTimer;  // 记录开始发光的时间点
            }
        }
        else {
            // 正在为生产而发光
            if (mProduceTimer >= mProductionGlowStartTimer + 0.55f) {
                // 2.5秒发光结束，生产阳光
                float offsetX = GameRandom::Range(-30.0f, 35.0f);
                mBoard->CreateSun(
                    this->mCurrectPosition.x + offsetX,
                    this->mCurrectPosition.y,
                    true);

                // 重置状态
                mProduceTimer = 0.0f;
                mIsGlowingForProduction = false;
                mProductionGlowStartTimer = 0.0f;
            }
            else {
                // 继续计时
                mProduceTimer += DeltaTime::GetDeltaTime();
            }
        }
    }

protected:
	void SetupPlant() override
	{
		Plant::SetupPlant();
	}
};

#endif
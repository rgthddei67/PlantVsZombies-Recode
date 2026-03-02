#pragma once
#ifndef _SUNFLOWER_H
#define _SUNFLOWER_H

#include "Plant.h"
#include "../Board.h"
#include "../../DeltaTime.h"

class SunFlower : public Plant
{
private:
    float mProduceTimer = 15.0f;
    float mProduceTime = 20.0f;
    bool mIsGlowingForProduction = false;  // 标记是否正在为生产发光
    float mProductionGlowStartTimer = 0.0f;  // 发光开始时间

public:
    using Plant::Plant;		

    void SaveExtraData(nlohmann::json& j) const override {
        j["produceTimer"] = mProduceTimer;
        j["isGlowingForProduction"] = mIsGlowingForProduction;
        j["productionGlowStartTimer"] = mProductionGlowStartTimer;
    }

    void LoadExtraData(const nlohmann::json& j) override {
        mProduceTimer = j.value("produceTimer", 15.0f);
        mIsGlowingForProduction = j.value("isGlowingForProduction", false);
        mProductionGlowStartTimer = j.value("productionGlowStartTimer", 0.0f);
    }

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
                Vector position = GetPosition();
                mBoard->CreateSun(
                    position.x + offsetX,
                    position.y,
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

};

#endif
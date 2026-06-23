#include "SunShroom.h"
#include "../Board.h"

void SunShroom::PlantUpdate()
{
	mGrowTimer += DeltaTime::GetDeltaTime();
	if (mGrowTimer >= GROW_TIME && !mIsGrown) {
		mGrowTimer = 0.0f;
		mIsGrown = true;
		PlayTrackOnce("anim_grow", "anim_bigidle", 0.0f, 0.2f, 0.0f);
	}

	if (!mIsGlowingForProduction) {
		// 正常计时生产
		mProduceTimer += DeltaTime::GetDeltaTime();
		if (mProduceTimer >= PRODUCE_TIME) {
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
			// 幼年产小阳光(15)，长大后产普通阳光(25)
			if (mIsGrown) {
				mBoard->CreateSun(position.x + offsetX, position.y, true);
			}
			else {
				mBoard->CreateSmallSun(position.x + offsetX, position.y, true);
			}

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
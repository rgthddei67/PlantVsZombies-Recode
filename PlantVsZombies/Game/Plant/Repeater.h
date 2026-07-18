#pragma once
#ifndef _REPEATER_H
#define _REPEATER_H

#include "Shooter.h"
#include "../Board.h"

class Repeater : public Shooter {
public:
	using Shooter::Shooter;

	/** 保存双发射手两发之间的瞬态，使读档后能准确续完当前连发。 */
	void SaveExtraData(nlohmann::json& j) const override {
		Shooter::SaveExtraData(j);
		j["pendingSecondShot"] = mPendingSecondShot;
		j["isSecondShot"] = mIsSecondShot;
	}

	/** 恢复连发瞬态；旧存档缺字段时按尚未发出第一颗豌豆处理。 */
	void LoadExtraData(const nlohmann::json& j) override {
		Shooter::LoadExtraData(j);
		mPendingSecondShot = j.value("pendingSecondShot", false);
		mIsSecondShot = j.value("isSecondShot", false);
	}

	void PlantUpdate() override {
		// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0）。
		float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;

		if (mPendingSecondShot) {
			mPendingSecondShot = false;
			// 补发（双发第二弹）：动画同样 * mult，与主发同步加快
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 2.0f * mult, 0.1f);
			return;
		}

		this->mShootTimer += DeltaTime::GetDeltaTime();
		if (this->mShootTimer >= this->mShootTime / mult)
		{
			if (HasZombieInRow())
			{
				mShootTimer = 0;
				// 动画同比例加快：吐弹的 frame event 跟上更短间隔
				mHeadAnim->PlayTrackOnce("anim_shooting", "", 1.9f * mult, 0.1f);
			}
		}
	}

protected:
	bool mPendingSecondShot = false;
	bool mIsSecondShot = false;

	void ShootBullet() override
	{
		if (!mBoard) return;
		Vector bulletPosition = GetPosition() + Vector(30, -30);
		mBoard->CreateBullet(BulletType::BULLET_PEA, mRow, bulletPosition);

		if (!mIsSecondShot) {
			mPendingSecondShot = true;
			mIsSecondShot = true;
		}
		else {
			mIsSecondShot = false;
		}
	}
};

#endif

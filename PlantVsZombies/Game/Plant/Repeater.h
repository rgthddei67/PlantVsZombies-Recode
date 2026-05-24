#pragma once
#ifndef _REPEATER_H
#define _REPEATER_H

#include "Shooter.h"
#include "../Board.h"

class Repeater : public Shooter {
public:
	using Shooter::Shooter;

	void PlantUpdate() override {
		Plant::PlantUpdate();

		if (mPendingSecondShot) {
			mPendingSecondShot = false;
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 2.0f, 0.1f);
			return;
		}

		this->mShootTimer += DeltaTime::GetDeltaTime();
		if (this->mShootTimer >= this->mShootTime)
		{
			if (HasZombieInRow())
			{
				mShootTimer = 0;
				mHeadAnim->PlayTrackOnce("anim_shooting", "", 1.9f, 0.1f);
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
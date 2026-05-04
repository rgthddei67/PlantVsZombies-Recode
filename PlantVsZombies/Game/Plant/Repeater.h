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
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 1.9f, 0.1f);
			mHeadAnim->AddFrameEvent(64, [this]() {
				if (GameRandom::Chance())
					AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.3f);
				else
					AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
				this->ShootBullet();
			});
			return;
		}

		this->mShootTimer += DeltaTime::GetDeltaTime();
		if (this->mShootTimer >= this->mShootTime)
		{
			if (HasZombieInRow())
			{
				mShootTimer = 0;
				mHeadAnim->PlayTrackOnce("anim_shooting", "", 1.9f, 0.2f);
				mHeadAnim->AddFrameEvent(64, [this]() {
					if (GameRandom::Chance())
						AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.3f);
					else
						AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
					this->ShootBullet();
					mPendingSecondShot = true;
				});
			}
		}
	}

protected:
	bool mPendingSecondShot = false;

	void ShootBullet() override
	{
		if (!mBoard) return;
		Vector bulletPosition = GetPosition() + Vector(30, -30);
		mBoard->CreateBullet(BulletType::BULLET_PEA, mRow, bulletPosition);
	}

};

#endif
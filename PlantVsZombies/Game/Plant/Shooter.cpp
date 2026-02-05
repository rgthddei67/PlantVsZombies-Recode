#include "Shooter.h"
#include "../Board.h"

void Shooter::SetupPlant()
{
	Plant::SetupPlant();
	auto animations = mCachedAnimator->GetAvailableAnimations();
	for (const auto& anim : animations) {
		std::cout << "可用动画: " << anim << std::endl;
	}

	// 播放指定动画
	mCachedAnimator->PlayTrack("anim_idle");
}

void Shooter::PlantUpdate()
{
	Plant::PlantUpdate();
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			ShootBullet();
		}
	}
}

bool Shooter::HasZombieInRow()
{
	if (mBoard)
	{
		// TODO: 实现检测本行是否有僵尸的逻辑
	}
	return true;
}
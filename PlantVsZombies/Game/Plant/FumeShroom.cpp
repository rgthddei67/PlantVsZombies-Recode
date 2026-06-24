#include "FumeShroom.h"
#include "../Board.h"

void FumeShroom::SetupPlant()
{
	Shroom::SetupPlant();

	if (mIsPreview) return;

	mAnimator->AddFrameEvent(27, [this]() {
		if (!mBoard) return;
		g_particleSystem->EmitEffect("FumeCloud", GetPosition());
		AudioSystem::PlaySound("SOUND_FUME", 0.28f);
		}, true);
}

void FumeShroom::PlantUpdate()
{
	// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0）。
	float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime / mult)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			// 动画同比例加快：吐弹的第 28 帧 frame event 跟上更短间隔
			PlayTrackOnce("anim_shooting", "anim_idle", 1.5f * mult, 0.2f);
		}
	}
}

bool FumeShroom::HasZombieInRow()
{
	if (mBoard)
	{
		mCheckZombieTimer += DeltaTime::GetDeltaTime();
		if (mCheckZombieTimer >= 0.6f)
		{
			mCheckZombieTimer = 0.0f;
			// 按行索引：只遍历本行僵尸，mRow 过滤已由桶保证。
			const float thisX = GetPosition().x;
			bool found = false;
			mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
				if (found) return;  // 已命中，跳过本行其余
				float dx = zombie->GetPosition().x - thisX;
				if (dx >= 0 && dx <= 380.0f)
					found = true;
				});
			return found;
		}
	}
	return false;
}
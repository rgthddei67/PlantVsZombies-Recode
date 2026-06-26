#include "PuffShroom.h"
#include "../ShadowComponent.h"

void PuffShroom::SetupPlant()
{
	auto shadow = GetComponent<ShadowComponent>();
	shadow->SetScale(Vector(0.6f, 0.6f));
	shadow->SetOffset(Vector(3, 30));

	Shroom::SetupPlant();

	if (mIsPreview) return;
	
	mAnimator->AddFrameEvent(28, [this]() {
		if (!mBoard) return;
		AudioSystem::PlaySound("SOUND_PUFF", 0.28f);
		Vector bulletPosition = GetPosition() + Vector(13, 13);
		mBoard->CreateBullet(BulletType::BULLET_PUFF, mRow, bulletPosition);
		}, true);
}

void PuffShroom::PlantUpdate()
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

bool PuffShroom::HasZombieInRow()
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
				if (dx >= 0 && dx <= 300.0f && zombie->HasHead())
					found = true;
			});
			return found;
		}
	}
	return false;
}
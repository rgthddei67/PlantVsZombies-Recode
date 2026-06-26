#include "FumeShroom.h"
#include "../Board.h"

namespace {
	constexpr int   kFumeDamage = 20;       // 每次喷射伤害（约一发豌豆，原版 DoRowAreaDamage(20, ...)）
	constexpr float kFumeReach = 380.0f;    // 喷雾横向覆盖范围（约 4 格锥形；检测与命中共用同一距离）
}

void FumeShroom::SetupPlant()
{
	Shroom::SetupPlant();

	if (mIsPreview) return;

	mAnimator->AddFrameEvent(27, [this]() {
		if (!mBoard) return;
		g_particleSystem->EmitEffect("FumeCloud", GetPosition());
		AudioSystem::PlaySound("SOUND_FUME", 0.28f);
		FumeAttack();   // 喷雾成形即攻击：对本行范围内全部僵尸造成穿透伤害
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
				if (dx >= 0 && dx <= kFumeReach && zombie->HasHead())
					found = true;
				});
			return found;
		}
	}
	return false;
}

void FumeShroom::FumeAttack()
{
	if (!mBoard) return;

	const float thisX = GetPosition().x;
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		const float dx = zombie->GetPosition().x - thisX;
		if (dx >= 0 && dx <= kFumeReach && zombie->HasHead())
			zombie->TakeDamage(kFumeDamage, /*penetrateShield=*/true);
		});
}
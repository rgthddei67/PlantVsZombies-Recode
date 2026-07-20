#include "FumeShroom.h"
#include "../Board.h"

void FumeShroom::SetupPlant()
{
	Shroom::SetupPlant();

	if (mIsPreview) return;

	mAnimator->AddFrameEvent(27, [this]() {
		if (!mBoard) return;
		g_particleSystem->EmitEffect(FumeParticleName(), GetPosition());
		AudioSystem::PlaySound("SOUND_FUME", 0.28f);
		FumeAttack();   // 喷雾成形即攻击：对本行范围内全部僵尸造成穿透伤害
		}, true);
}

void FumeShroom::PlantUpdate()
{
	// 生存攻速词条 × 雨势行动倍率；喷雾结算帧和攻击间隔必须同比例提前。
	float mult = GetAttackSpeedMultiplier();
	this->mShootTimer += (DeltaTime::GetDeltaTime() * mult);
	if (this->mShootTimer >= this->mShootTime)
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
				// 跳过魅惑僵尸：全行只剩魅惑时不触发喷射动画（与 Chomper/PotatoMine 索敌跳过魅惑同一惯例）
				if (!zombie->IsMindControlled() && dx >= 0 && dx <= mFumeReach && zombie->HasHead() && !zombie->IsMindControlled())
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
		// 豁免魅惑僵尸：原版 DoRowAreaDamage(20, 2U) 的 damageRangeFlags 不含 bit7（不炸魅惑目标）
		if (dx >= 0 && dx <= mFumeReach && zombie->HasHead() && !zombie->IsMindControlled())
		{
			zombie->TakeDamage(mFumeDamage, DamageSource::PLANT, /*penetrateShield=*/true);
			OnFumeHit(zombie);
		}
		});
}

#include "DoorZombie.h"
#include "../Plant/Plant.h"

void DoorZombie::SetupZombie()
{
	this->ShowArm(false);

	if (mIsPreview) return;

	mAnimator->AddFrameEvent(216, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(152, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(171, [this]() { this->EatTarget(); }, true);

	this->mShieldHealth = 1100;
	this->mShieldMaxHealth = 1100;
	this->mShieldType = ShieldType::SHIELDTYPE_DOOR;

	mSpeed += GameRandom::Range(-3, 3);

	if (GameRandom::Range(0, 1) == 0)
		this->PlayTrack("anim_walk");
	else
		this->PlayTrack("anim_walk2");
}

void DoorZombie::StartEat(ColliderComponent* other)
{
	if (mIsPreview || mIsDying)	return;
	if (other->GetGameObject()->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
		const bool wasEating = mIsEating;
		Zombie::StartEat(other);
		// 魅惑后啃敌方僵尸也要像啃植物一样露出常规手臂（否则啃食时无手臂）。只在本次真正开吃
		// （base 置了 mIsEating）且门还在时露臂，与植物分支 / ResumeWalkAfterEat 收尾对称。
		if (!wasEating && mIsEating && mShieldType != ShieldType::SHIELDTYPE_NONE)
			this->ShowArm(true);
		return;
	}
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != NULL_PLANT_ID || plant->mRow != this->mRow) return;	// 正在吃一个植物，那么不吃别的植物

			if (!mIsEating) {
				this->PlayTrack("anim_eat", 2.1f, 0.2f);
				if (this->mShieldType != ShieldType::SHIELDTYPE_NONE) {
					this->ShowArm(true);
				}
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
			plant->mEaterCount++;
		}
	}
}

void DoorZombie::StopEat(ColliderComponent* other)
{
	if (mIsPreview || mIsDying)	return;
	if (other->GetGameObject()->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
		Zombie::StopEat(other);
		return;
	}
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->PlayTrack("anim_walk2", 0.0f, 0.2f);   // clip 清零，自动回落走速

				if (this->mShieldType != ShieldType::SHIELDTYPE_NONE) {
					this->ShowArm(false);
				}
				plant->mEaterCount--;
			}
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
		}
	}
}

void DoorZombie::ValidateEatingState(EntityManager& em)
{
	if (mIsEating && mEatPlantID != NULL_PLANT_ID) {
		auto plant = em.GetPlant(mEatPlantID);
		if (!plant) {
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;

			if (this->mShieldType != ShieldType::SHIELDTYPE_NONE) {
				this->ShowArm(false);
			}
			
			PlayTrack("anim_walk2", 0.0f, 0.3f);   // clip 清零，自动回落走速
		}
		else {
			plant->mEaterCount++;
		}
	}
	else if (mIsEating) {
		// mEatPlantID 为空却在啃：啃僵尸进行时存的档（mEatZombieID 不持久化）→ 回走路，碰撞下一帧重建互啃
		mIsEating = false;
		PlayTrack(WalkTrackAfterEat(), 0.0f, 0.3f);
	}
}

void DoorZombie::ResumeWalkAfterEat(float blendTime)
{
	Zombie::ResumeWalkAfterEat(blendTime);   // 播 WalkTrackAfterEat()="anim_walk2"（铁门有该轨道）

	// 啃食时 StartEat 露过常规手臂；回走路要藏回门后。门已掉（mShieldType==NONE）则手臂应保持
	// 可见，故用与 StopEat / ValidateEatingState 相同的守卫。修复"铁门僵尸被魅惑后多一条手臂"。
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
		this->ShowArm(false);
}

void DoorZombie::ShowArm(bool show) const
{
	if (!mAnimator) return;
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", show);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", show);
	mAnimator->SetTrackVisible("Zombie_outerarm_upper", show);
	mAnimator->SetTrackVisible("anim_innerarm1", show);
	mAnimator->SetTrackVisible("anim_innerarm2", show);
	mAnimator->SetTrackVisible("anim_innerarm3", show);
}

void DoorZombie::ShowBrokenArm() const
{
	if (!mAnimator) return;
	// 该亮的：内臂 + 外上臂；该灭的：外手掌 + 外下臂（断臂只剩上臂残端）。
	mAnimator->SetTrackVisible("anim_innerarm1", true);
	mAnimator->SetTrackVisible("anim_innerarm2", true);
	mAnimator->SetTrackVisible("anim_innerarm3", true);
	mAnimator->SetTrackVisible("Zombie_outerarm_upper", true);
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));

	// 断臂即抓不住门：把「持门手臂」一并隐藏。正常死亡时门先掉、ShieldDrop 已隐藏它们（此处无害 no-op）；
	// 但大喷菇穿透会在门未掉时打死本体（掉头 HeadDrop 走到这里），若不隐藏就残留一条完好的持门手臂。
	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor_hand", false);
}

void DoorZombie::CheckShieldImage()
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE) return;

	if (mShieldStage == ArmorBrokenState::NO_BROKEN && mShieldHealth <= static_cast<int64_t>(mShieldMaxHealth) * 2 / 3) {
		mShieldStage = ArmorBrokenState::A_LITTLE_BROKEN;

		mAnimator->SetTrackImage("anim_screendoor", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_SCREENDOOR2"));
	}
	if (mShieldStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mShieldHealth <= mShieldMaxHealth / 3) {
		mShieldStage = ArmorBrokenState::REALLY_BROKEN;

		mAnimator->SetTrackImage("anim_screendoor", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_SCREENDOOR3"));
	}
}

void DoorZombie::ShieldDrop()
{
	Zombie::ShieldDrop();
	mShieldStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("anim_screendoor", false);

	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor_hand", false);
	
	this->ShowArm(true);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieDoorOff",
			GetPosition());
	}
} 

void DoorZombie::TakeBodyDamage(int damage)
{
	mBodyHealth -= damage;
	if (mBodyHealth < 0)
		mBodyHealth = 0;

	if (mNeedDropArm && mHasArm && mShieldType == ShieldType::SHIELDTYPE_NONE 
		&& mBodyHealth <= static_cast<int64_t>(mBodyMaxHealth) * 2 / 3)
	{
		ArmDrop();
		mHasArm = false;
	}

	if (mNeedDropHead && mHasHead && mBodyHealth <= mBodyMaxHealth / 3)
	{
		HeadDrop();
		mHasHead = false;
	}
}

void DoorZombie::HeadDrop()
{
	if (!mHasHead) return;

	// 显示手臂，并且让它为断裂的造型
	if (mHasArm) {
		this->ShowBrokenArm();
	}

	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect("ZombieHeadOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}
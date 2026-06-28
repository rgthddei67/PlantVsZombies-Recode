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
}

bool DoorZombie::ShowArm(bool show) const
{
	if (mAnimator) {
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", show);
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", show);
		mAnimator->SetTrackVisible("Zombie_outerarm_upper", show);
		mAnimator->SetTrackVisible("anim_innerarm1", show);
		mAnimator->SetTrackVisible("anim_innerarm2", show);
		mAnimator->SetTrackVisible("anim_innerarm3", show);
		return true;
	}
	else {
		return false;
	}
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

	// 显示手臂，并且让它为断裂的动画
	if (mHasArm) {
		this->ShowArm(true);
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
			GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
	}

	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect("ZombieHeadOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}
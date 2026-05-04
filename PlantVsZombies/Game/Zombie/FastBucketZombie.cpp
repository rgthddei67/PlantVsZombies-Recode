#include "FastBucketZombie.h"

void FastBucketZombie::SetupZombie()
{
	Zombie::SetupZombie();
	mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
		GetTexture("IMAGE_FASTZOMBIE_BUCKET1"));

	if (mIsPreview) return;
	this->mHelmHealth = 620;
	this->mHelmMaxHealth = 620;
	this->mHelmType = HelmType::HELMTYPE_BUCKET;
	this->mSpeed *= GameRandom::Range(2.1f, 3.0f);
	this->mAttackDamage *= 3;
	mExtraSpeed = GameRandom::Range(1.8f, 2.9f);
	mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);
}

void FastBucketZombie::HelmDrop()
{
	Zombie::HelmDrop();
	mHelmStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("anim_bucket", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieFastBucketOff",
			GetPosition());
	}
}

void FastBucketZombie::CheckHelmImage()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	if (mHelmStage == ArmorBrokenState::NO_BROKEN && mHelmHealth <= mHelmMaxHealth * 2 / 3) {
		mHelmStage = ArmorBrokenState::A_LITTLE_BROKEN;
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_FASTZOMBIE_BUCKET2"));
	}
	if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mHelmHealth <= mHelmMaxHealth / 3) {
		mHelmStage = ArmorBrokenState::REALLY_BROKEN;
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_FASTZOMBIE_BUCKET3"));
	}
}
#include "BucketZombie.h"

void BucketZombie::SetupZombie()
{
	Zombie::SetupZombie();
	this->mHelmHealth = 1100;
	this->mHelmMaxHealth = 1100;
	this->mHelmType = HelmType::HELMTYPE_BUCKET;
}

void BucketZombie::HelmDrop()
{
	Zombie::HelmDrop();
	mHelmStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("anim_bucket", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieBucketOff",
			GetPosition());
	}
}

void BucketZombie::CheckHelmImage()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	if (mHelmStage == ArmorBrokenState::NO_BROKEN && mHelmHealth <= mHelmMaxHealth * 2 / 3) {
		mHelmStage = ArmorBrokenState::A_LITTLE_BROKEN;
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_BUCKET2"));
	}
	if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mHelmHealth <= mHelmMaxHealth / 3) {
		mHelmStage = ArmorBrokenState::REALLY_BROKEN;
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_BUCKET3"));
	}
}
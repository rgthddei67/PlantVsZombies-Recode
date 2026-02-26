#include "ConeZombie.h"

void ConeZombie::SetupZombie()
{
	this->mHelmHealth = 370;
	this->mHelmMaxHealth = 370;
	this->mHelmType = HelmType::HELMTYPE_TRAFFIC_CONE;
}

void ConeZombie::HelmDrop()
{
	Zombie::HelmDrop();
	mHelmStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("anim_cone", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect(ParticleType::ZOMBIE_CONE_OFF, 
			GetPosition() + Vector(0, -80));
	}
}

void ConeZombie::CheckHelmImage()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	if (mHelmStage == ArmorBrokenState::NO_BROKEN && mHelmHealth <= mHelmMaxHealth * 2 / 3) {
		mHelmStage = ArmorBrokenState::A_LITTLE_BROKEN;
		mAnimator->SetTrackImage("anim_cone", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_CONE2"));
	}
	if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mHelmHealth <= mHelmMaxHealth / 3) {
		mHelmStage = ArmorBrokenState::REALLY_BROKEN;
		mAnimator->SetTrackImage("anim_cone", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_CONE3"));
	}
}
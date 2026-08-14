#include "ConeZombie.h"

void ConeZombie::SetupZombie()
{
	Zombie::SetupZombie();
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
		g_particleSystem->EmitEffect("ZombieConeOff",
			GetPosition());
	}
}

void ConeZombie::CheckHelmImage()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	mHelmStage = mHelmHealth > static_cast<int64_t>(mHelmMaxHealth) * 2 / 3
		? ArmorBrokenState::NO_BROKEN
		: (mHelmHealth > mHelmMaxHealth / 3
			? ArmorBrokenState::A_LITTLE_BROKEN : ArmorBrokenState::REALLY_BROKEN);
	const char* imageKey = mHelmStage == ArmorBrokenState::NO_BROKEN
		? "IMAGE_ZOMBIE_CONE1"
		: (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN
			? "IMAGE_ZOMBIE_CONE2" : "IMAGE_ZOMBIE_CONE3");
	mAnimator->SetTrackImage("anim_cone",
		ResourceManager::GetInstance().GetTexture(imageKey));
}

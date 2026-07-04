#include "FootballZombie.h"
#include "../AudioSystem.h"

void FootballZombie::SetupZombie()
{
	this->mHelmHealth = 1100;
	this->mHelmMaxHealth = 1100;
	this->mHelmType = HelmType::HELMTYPE_FOOTBALL;

	if (!mIsPreview) {
		this->mSpeed *= 1.7f;
		mExtraSpeed = 1.8f;
		mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);

		PlayTrack("anim_walk");

		mAnimator->AddFrameEvent(63, [this]() {
			this->EatTarget();
			}, true);
		mAnimator->AddFrameEvent(80, [this]() {
			this->EatTarget();
			}, true);
		mAnimator->AddFrameEvent(104, [this]() {
			this->Die();
			});
	}
}

void FootballZombie::CheckHelmImage()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	if (mHelmStage == ArmorBrokenState::NO_BROKEN && mHelmHealth <= static_cast<int64_t>(mHelmMaxHealth) * 2 / 3) {
		mHelmStage = ArmorBrokenState::A_LITTLE_BROKEN;
		mAnimator->SetTrackImage("zombie_football_helmet", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_FOOTBALL_HELMET2"));
	}
	if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mHelmHealth <= mHelmMaxHealth / 3) {
		mHelmStage = ArmorBrokenState::REALLY_BROKEN;
		mAnimator->SetTrackImage("zombie_football_helmet", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_FOOTBALL_HELMET3"));
	}
}

void FootballZombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect("FootballZombieHeadOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void FootballZombie::HelmDrop()
{
	Zombie::HelmDrop();
	mHelmStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("zombie_football_helmet", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieFootballOff",
			GetPosition());
	}
}

void FootballZombie::ArmDrop()
{
	if (!mHasArm) return;
	mAnimator->SetTrackVisible("zombie_football_leftarm_hand", false);
	mAnimator->SetTrackVisible("zombie_football_leftarm_lower", false);
	mAnimator->SetTrackImage("zombie_football_leftarm_upper", ResourceManager::GetInstance().
		GetTexture("IMAGE_ZOMBIE_FOOTBALL_LEFTARM_UPPER2"));
	g_particleSystem->EmitEffect("FootballZombieArmOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}
#pragma once
#ifndef _FOOTBALLZOMBIE_H_
#define _FOOTBALLZOMBIE_H_

#include "Zombie.h"

class FootballZombie : public Zombie {
protected:
	void SetupZombie() override;
	void CheckHelmImage() override;

public:
	using Zombie::Zombie;
	ArmorBrokenState mHelmStage = ArmorBrokenState::NO_BROKEN;

	void HelmDrop() override;
	void HeadDrop() override;
	void ArmDrop() override;

	void SaveExtraData(nlohmann::json& j) const override {
		j["helmStage"] = static_cast<int>(mHelmStage);
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mHelmStage = static_cast<ArmorBrokenState>(
			j.value("helmStage", static_cast<int>(ArmorBrokenState::NO_BROKEN))
			);
	}

	void ZombieItemUpdate() const override {
		if (!mHasArm) {
			mAnimator->SetTrackVisible("zombie_football_leftarm_hand", false);
			mAnimator->SetTrackVisible("zombie_football_leftarm_lower", false);
			mAnimator->SetTrackImage("zombie_football_leftarm_upper", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_FOOTBALL_LEFTARM_UPPER2"));
		}
		if (!mHasHead) {
			mAnimator->SetTrackVisible("anim_head1", false);
			mAnimator->SetTrackVisible("anim_head2", false);
			mAnimator->SetTrackVisible("anim_hair", false);
		}

		if (mHelmStage == ArmorBrokenState::NONE || mHelmType == HelmType::HELMTYPE_NONE) {
			mAnimator->SetTrackVisible("zombie_football_helmet", false);
		}
		else if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN) {
			mAnimator->SetTrackImage("zombie_football_helmet", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_FOOTBALL_HELMET2"));
		}
		else if (mHelmStage == ArmorBrokenState::REALLY_BROKEN) {
			mAnimator->SetTrackImage("zombie_football_helmet", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_FOOTBALL_HELMET3"));
		}
	}

	void PlayWalkAnimation(float blendTime = 0.0f) override
	{
		PlayTrack("anim_walk", 0.0f, blendTime);
	}


};

#endif
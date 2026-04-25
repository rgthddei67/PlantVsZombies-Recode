#pragma once
#ifndef _BUCKET_ZOMBIE_H
#define _BUCKET_ZOMBIE_H
#include "Zombie.h"

class BucketZombie : public Zombie {
public:
	using Zombie::Zombie;
	ArmorBrokenState mHelmStage = ArmorBrokenState::NO_BROKEN;

	void HelmDrop() override;

	void SaveExtraData(nlohmann::json& j) const override {
		j["helmStage"] = static_cast<int>(mHelmStage);
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mHelmStage = static_cast<ArmorBrokenState>(
			j.value("helmStage", static_cast<int>(ArmorBrokenState::NO_BROKEN))
			);
	}

	void ZombieItemUpdate() const override {
		Zombie::ZombieItemUpdate();

		if (mHelmStage == ArmorBrokenState::NONE || mHelmType == HelmType::HELMTYPE_NONE) {
			mAnimator->SetTrackVisible("anim_bucket", false);
		}
		else if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN) {
			mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_BUCKET2"));
		}
		else if (mHelmStage == ArmorBrokenState::REALLY_BROKEN) {
			mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_BUCKET3"));
		}
	}

protected:
	void SetupZombie() override;
	void CheckHelmImage() override;
};


#endif
#pragma once
#ifndef _FASTBUCKET_ZOMBIE_H
#define _FASTBUCKET_ZOMBIE_H

#include "Zombie.h"

class FastBucketZombie : public Zombie {
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
				GetTexture("IMAGE_FASTZOMBIE_BUCKET2"));
		}
		else if (mHelmStage == ArmorBrokenState::REALLY_BROKEN) {
			mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
				GetTexture("IMAGE_FASTZOMBIE_BUCKET3"));
		}
	}

	void SetCooldown(float timer) override {
		if (!mAnimator || mShieldType != ShieldType::SHIELDTYPE_NONE) return;

		if (mCooldownTimer <= 0.0f)
		{
			mAnimator->SetExtraSpeedMultiplier(mExtraSpeed * 0.8f);
			mAnimator->EnableOverlayEffect(true);
			mAnimator->SetOverlayColor(80, 80, 255, 240);
		}

		mCooldownTimer = std::max(mCooldownTimer, timer);
	}

	void TakeDamage(int damage, bool penetrateShield) override {
		if (GameRandom::Range(1, 10) <= 2) return;
		Zombie::TakeDamage(damage, penetrateShield);
	}

protected:
	void SetupZombie() override;
	void CheckHelmImage() override;
};

#endif
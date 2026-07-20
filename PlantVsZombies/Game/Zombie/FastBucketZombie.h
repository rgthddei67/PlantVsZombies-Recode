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

	// 减速动画只降到 0.8x（快速僵尸减速后仍偏快的手感）；其余逻辑沿用基类 SetCooldown/UpdateAnimSpeed
	float GetSlowAnimFactor() const override { return 0.8f; }

	void TakeDamage(int damage, DamageSource source, bool penetrateShield) override {
		if (GameRandom::Range(1, 10) <= 2) return;
		Zombie::TakeDamage(damage, source, penetrateShield);
	}

protected:
	void SetupZombie() override;
	void CheckHelmImage() override;
};

#endif

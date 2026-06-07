#pragma once
#ifndef _PARERZOMBIE_H_
#define _PARERZOMBIE_H_

#include "Zombie.h"

class PaperZombie : public Zombie {
protected:
	bool mHasNewspaper = true;	// 是否有报纸
	bool mIsGasp = false;	// 发怒动画

	ArmorBrokenState mShieldStage = ArmorBrokenState::NO_BROKEN;	

	void SetupZombie() override;

	void ValidateEatingState(EntityManager& em) override;
	void StartEat(ColliderComponent* other) override;
	void StopEat(ColliderComponent* other) override;
	void HeadDrop() override;
	void ArmDrop() override;
	void ShieldDrop() override;

	void CheckShieldImage() override;

	void ZombieMove(float scaledDelta, TransformComponent* transform) override;

public:
	using Zombie::Zombie;

	void ZombieItemUpdate() const override {
		if (!mHasArm) {
			mAnimator->SetTrackVisible("Zombie_paper_hands", false);
			mAnimator->SetTrackVisible("Zombie_paper_leftarm_lower", false);
			mAnimator->SetTrackImage("Zombie_paper_leftarm_upper", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_PAPER_LEFTARM_UPPER2"));
		}
		if (!mHasHead) {
			mAnimator->SetTrackVisible("anim_head1", false);
			mAnimator->SetTrackVisible("anim_head_pupils", false);
			mAnimator->SetTrackVisible("anim_head_look", false);
			mAnimator->SetTrackVisible("anim_hairpiece", false);
			mAnimator->SetTrackVisible("anim_head_jaw", false);
			mAnimator->SetTrackVisible("anim_head_glasses", false);
			mAnimator->SetTrackVisible("anim_hair", false);
		}

		if (mShieldStage == ArmorBrokenState::NONE || mShieldType == ShieldType::SHIELDTYPE_NONE) {
			mAnimator->SetTrackVisible("Zombie_paper_paper", false);
		}
		else if (mShieldStage == ArmorBrokenState::A_LITTLE_BROKEN) {
			mAnimator->SetTrackImage("Zombie_paper_paper", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_PAPER_PAPER2"));
		}
		else if (mShieldStage == ArmorBrokenState::REALLY_BROKEN) {
			mAnimator->SetTrackImage("Zombie_paper_paper", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_PAPER_PAPER3"));
		}
	}

	void SaveExtraData(nlohmann::json& j) const override {
		j["shieldStage"] = static_cast<int>(mShieldStage);
		j["hasNewspaper"] = mHasNewspaper;
		j["isGasp"] = mIsGasp;   // extra 速度层基准已由基类 Zombie::SaveProtectedData 统一持久化
	}

	void LoadExtraData(const nlohmann::json& j) override;

};

#endif
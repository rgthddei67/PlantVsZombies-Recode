#pragma once
#ifndef _DOOR_ZOMBIE_H
#define _DOOR_ZOMBIE_H

#include "Zombie.h"

class DoorZombie : public Zombie {
protected:
	bool mHasDoor = true;	// 是否有铁门

	ArmorBrokenState mShieldStage = ArmorBrokenState::NO_BROKEN;

	void SetupZombie() override;

	void ShieldDrop() override;

	void CheckShieldImage() override;

	// 是否显示手臂 true等于设置成功，false等于设置失败（没有animator）
	virtual bool ShowArm(bool show) const;

public:
	using Zombie::Zombie;

	void ValidateEatingState(EntityManager& em) override;
	void StartEat(ColliderComponent* other) override;
	void StopEat(ColliderComponent* other) override;

	void HeadDrop() override;

	void ZombieItemUpdate() const override {
		if (mShieldStage == ArmorBrokenState::NONE || mShieldType == ShieldType::SHIELDTYPE_NONE) {
			mAnimator->SetTrackVisible("anim_screendoor", false);
			mAnimator->SetTrackVisible("Zombie_innerarm_screendoor", false);
			mAnimator->SetTrackVisible("Zombie_outerarm_screendoor", false);
			mAnimator->SetTrackVisible("Zombie_innerarm_screendoor_hand", false);
			this->ShowArm(true);
		}
		else if (mShieldStage == ArmorBrokenState::A_LITTLE_BROKEN) {
			mAnimator->SetTrackImage("anim_screendoor", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_SCREENDOOR2"));
		}
		else if (mShieldStage == ArmorBrokenState::REALLY_BROKEN) {
			mAnimator->SetTrackImage("anim_screendoor", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_SCREENDOOR3"));
		}

		if (!mHasArm) {
			mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
			mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
			mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
				GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
		}
		if (!mHasHead) {
			mAnimator->SetTrackVisible("anim_head1", false);
			mAnimator->SetTrackVisible("anim_head2", false);
			mAnimator->SetTrackVisible("anim_hair", false);
		}


		if (mIsEating && mShieldType != ShieldType::SHIELDTYPE_NONE) {
			this->ShowArm(true);
		}
		if (mIsDying) {
			this->ShowArm(true);
			mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
			mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
			mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
				GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
		}
	}

	void SaveExtraData(nlohmann::json& j) const override {
		j["shieldStage"] = static_cast<int>(mShieldStage);
		j["hasDoor"] = mHasDoor;
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mShieldStage = static_cast<ArmorBrokenState>(
			j.value("shieldStage", static_cast<int>(ArmorBrokenState::NO_BROKEN)));
		mHasDoor = j.value("hasDoor", true);
	}

	void TakeBodyDamage(int damage) override;
};

#endif
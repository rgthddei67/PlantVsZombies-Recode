#pragma once
#ifndef _POLEVAULTER_H
#define _POLEVAULTER_H

#include "Zombie.h"

class Polevaulter : public Zombie {
public:
	using Zombie::Zombie;

	enum class VaultState {
		RUNNING,	// 手持撑杆跑步
		JUMPING,	// 跳跃中
		WALKING		// 跳跃后走路
	};

	VaultState mVaultState = VaultState::RUNNING;
	bool mHasVaulted = false;

	void StopEat(ColliderComponent* other) override;
	void StartEat(ColliderComponent* other) override;

	void StartJump();
	void EndJump();
	void JumpMove(float distance);  // 跳跃期间手动移动，传入本次移动距离

	void ValidateEatingState(EntityManager& em) override;

	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;

	// 撑杆跑/跳阶段不可魅惑（原版：跳过魅惑菇根本不吃它）；落地 WALKING 后可
	bool CanBeCharmed() const override { return mVaultState == VaultState::WALKING; }

protected:
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	void SetupZombie() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;

	const char* WalkTrackAfterEat() const override { return "anim_walk"; }
};

#endif

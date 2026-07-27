#pragma once

#include "Zombie.h"

class Plant;

/**
 * @brief 经典海豚僵尸：只在泳池水路生成，骑豚越过第一株植物后弃豚游泳。
 */
class DolphinRiderZombie final : public Zombie {
public:
	using Zombie::Zombie;

	enum class Phase {
		APPROACHING,
		ENTERING_POOL,
		RIDING,
		JUMPING,
		SWIMMING,
		WALKING_WITHOUT_DOLPHIN,
	};

	Phase GetPhase() const { return mPhase; }
	bool HasDolphin() const;
	bool HasCheckedJumpBlocker() const { return mJumpBlockChecked; }
	float GetJumpProgress() const;
	/** 返回为消除海豚各阶段 reanim 基准差而叠加的连续视觉补偿。 */
	Vector GetDolphinVisualCompensation() const;

	void StartEat(ColliderComponent* other) override;
	void HeadDrop() override;
	void ArmDrop() override;
	void PlaySpawnSound() override;
	void ZombieItemUpdate() const override;
	Vector GetVisualPosition() const override;
	bool CanBeCharmed() const override;
	bool CanBeFrozen() const override;
	bool CanBeGrabbedByTangleKelp() const override;

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	void ZombieUpdate(float scaledTime) override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	void PlayWalkAnimation(float blendTime) override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	bool ShouldPlayDeathAnimation() const override;

private:
	void BeginEnteringPool();
	void FinishEnteringPool();
	void BeginJump(Plant* target);
	void FinishJump(bool blocked);
	void ApplyPhasePresentation() const;
	void CheckDeferredArmDrop();
	void MoveManually(float speed, float scaledDelta, TransformComponent* transform);

	Phase mPhase = Phase::APPROACHING;
	int mJumpTargetPlantID = NULL_PLANT_ID;
	bool mEntrySplashPlayed = false;
	bool mJumpSplashPlayed = false;
	bool mJumpBlockChecked = false;
	float mBaseColliderOffsetX = -25.0f;
};

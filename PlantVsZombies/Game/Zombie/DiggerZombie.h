#pragma once

#include "Zombie.h"

/**
 * @brief 经典矿工僵尸：地下穿行至房屋前出土，眩晕后向前线折返；丢镐时走独立出土分支。
 */
class DiggerZombie final : public Zombie {
public:
	using Zombie::Zombie;
	~DiggerZombie() override;

	enum class Phase {
		TUNNELING,
		RISING,
		STUNNED,
		WALKING_WITH_PICKAXE,
		TUNNELING_PAUSE_WITHOUT_PICKAXE,
		RISING_WITHOUT_PICKAXE,
		WALKING_WITHOUT_PICKAXE,
	};

	Phase GetPhase() const { return mPhase; }
	float GetPhaseRemaining() const { return mPhaseRemaining; }
	float GetAltitude() const { return mAltitude; }
	bool HasPickaxe() const { return mHasPickaxe; }
	bool HasShownSurprise() const { return mSurpriseShown; }
	/** 磁力菇与 AutoTest 共用的原版丢镐入口。 */
	void LosePickaxe();

	void PlaySpawnSound() override;
	void StartEat(ColliderComponent* other) override;
	void HelmDrop() override;
	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;
	void TakePlantAshDamage(int damage) override;
	void Charred() override;
	void Die() override;
	bool IsMovingRight() const override;
	bool CanTriggerGameOver() const override;
	bool CanBeTargetedByProjectile(bool targetsFlying) const override;
	bool CanBeCharmed() const override;
	bool CanBeChilled() const override;
	bool CanBeFrozen() const override;
	bool CanBeCharred() const override;
	bool CanBeGrabbedByTangleKelp() const override { return false; }

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	void ZombieUpdate(float scaledTime) override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	void PlayWalkAnimation(float blendTime) override;
	void OnStartEating() override;
	void CheckHelmImage() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	float GetAbilityAnimSpeedMultiplier() const override;
	bool CanUseGroundPoolState() const override { return false; }
	bool TryGetDrawClipBottom(float& clipBottom) const override;

private:
	void BeginRising(bool withPickaxe);
	void UpdateRising(float scaledTime, bool withPickaxe);
	void BeginStableWalk(bool withPickaxe);
	void ApplyPhasePresentation();
	void ApplyAltitude();
	void UpdateFacing();
	void EmitTunnelDust();
	void EmitRiseEffects(bool playWakeup);
	void StopEatingForTransition();
	void ClaimLoopSound();
	void ReleaseLoopSound();
	float GetWalkClipSpeed(float velocity) const;
	bool IsInteractivePhase() const;
	Vector GetStableVisualOrigin() const;

	Phase mPhase = Phase::TUNNELING;
	float mPhaseRemaining = 0.0f;
	float mAltitude = 0.0f;
	float mBaseVisualOffsetY = 0.0f;
	float mTunnelVelocity = 0.67f;
	float mWalkVelocity = 0.30f;
	float mTunnelDustTimer = 0.0f;
	bool mHasPickaxe = true;
	bool mSurpriseShown = false;
	bool mLandingStarted = false;
	bool mLoopSoundClaimed = false;
	ArmorBrokenState mHelmStage = ArmorBrokenState::NO_BROKEN;

	static int sLoopSoundUsers;
};

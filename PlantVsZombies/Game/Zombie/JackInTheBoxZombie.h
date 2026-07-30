#pragma once

#include "Zombie.h"

/**
 * @brief 经典小丑僵尸：伴随手摇盒循环声快速前进，随机倒计时后开盒并范围爆炸。
 */
class JackInTheBoxZombie : public Zombie {
public:
	using Zombie::Zombie;
	~JackInTheBoxZombie() override;

	enum class Phase {
		RUNNING,
		POPPING,
	};

	Phase GetPhase() const { return mPhase; }
	float GetPopCountdown() const { return mPopCountdown; }
	bool HasPlayedSurprise() const { return mSurprisePlayed; }
	bool HasResolvedExplosion() const { return mExplosionResolved; }
	float GetRunVelocity() const { return mRunVelocity; }
	/** AutoTest 用确定性入口；仅在 RUNNING 阶段覆盖剩余开盒秒数。 */
	void SetPopCountdownForTesting(float seconds);

	void Update() override;
	void TakeDamage(int damage, DamageSource source, bool penetrateShield = false,
		bool discardShieldOverflow = false) override;
	void StartEat(ColliderComponent* other) override;
	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;
	void PlaySpawnSound() override;
	void Die() override;
	bool CanBeFrozen() const override;

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	void PlayWalkAnimation(float blendTime) override;
	void OnStartEating() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	float GetAbilityAnimSpeedMultiplier() const override;

private:
	void BeginPop();
	void PlaySurprise();
	void Explode();
	void StopEatingForPop();
	void ClaimLoopSound();
	void ReleaseLoopSound();
	Vector GetExplosionCenter() const;

	Phase mPhase = Phase::RUNNING;
	float mPopCountdown = 0.0f;
	float mSurpriseCountdown = 0.0f;
	float mRunVelocity = 0.67f;
	bool mSurprisePlayed = false;
	bool mExplosionResolved = false;
	bool mLoopSoundClaimed = false;

	static int sLoopSoundUsers;
};

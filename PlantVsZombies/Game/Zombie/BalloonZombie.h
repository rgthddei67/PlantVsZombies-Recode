#pragma once

#include "Zombie.h"

/**
 * @brief 经典气球僵尸：先以 20 点气球生命飞行，破裂演出结束后落地并恢复普通啃食。
 */
class BalloonZombie final : public Zombie {
public:
	using Zombie::Zombie;

	enum class Phase {
		FLYING,
		POPPING,
		WALKING,
	};

	Phase GetPhase() const { return mPhase; }
	int GetBalloonHealth() const { return mBalloonHealth; }
	int GetBalloonMaxHealth() const { return mBalloonMaxHealth; }
	float GetFlightVelocity() const { return mFlightVelocity; }
	float GetPropellerFrame() const;
	bool IsPropellerPlaying() const;
	/** AutoTest 静止靶与存档恢复共用的明确速度入口，单位：像素/秒。 */
	void SetFlightVelocity(float velocity);

	void Update() override;
	void StartEat(ColliderComponent* other) override;
	void PlaySpawnSound() override;
	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;
	bool IsFlying() const override { return mPhase != Phase::WALKING; }
	bool CanBeTargetedByProjectile(bool targetsFlying) const override;
	bool CanBeFrozen() const override { return mPhase == Phase::WALKING; }
	bool CanBeCharred() const override {
		return mPhase == Phase::WALKING && Zombie::CanBeCharred();
	}

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	void ZombieUpdate(float scaledTime) override;
	void PlayWalkAnimation(float blendTime) override;
	void OnStartEating() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	int TakeExtraProtectionDamage(int damage, DamageSource source) override;
	void ApplyExtraHealthMultiplier(double multiplier) override;
	bool CanUseGroundPoolState() const override { return mPhase == Phase::WALKING; }

private:
	/** 从同一 reanim 创建只循环 propeller 轨道的帽子附件。 */
	void CreatePropellerAnimator();
	/** 清除父轨切换传播来的 clip 覆盖，同时保留冻结和减速的 extra 层。 */
	void KeepPropellerIndependent() const;
	/** 消耗完气球生命后播放爆裂演出；水路行直接移除。 */
	void PopBalloon();
	/** 爆裂轨结束后提交地面阶段、碰撞框和走路轨道。 */
	void FinishLanding();
	/** 按当前阶段同步碰撞高度、入水状态和断肢许可。 */
	void ApplyPhasePresentation();
	/** 落地时补结算气球阶段积累的本体断肢断头阈值。 */
	void ResolveDeferredBodyParts();

	Phase mPhase = Phase::FLYING;
	int mBalloonHealth = 20;
	int mBalloonMaxHealth = 20;
	float mFlightVelocity = 30.0f;
	float mGroundColliderOffsetY = -65.0f;
	std::shared_ptr<Animator> mPropellerAnimator;
};

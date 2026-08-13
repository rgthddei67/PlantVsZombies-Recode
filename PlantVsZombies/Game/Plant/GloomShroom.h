#pragma once

#include "Shroom.h"

class Zombie;

/**
 * 原版忧郁菇：每两秒发起一次环形攻击，并在一次攻击中结算四段范围伤害。
 * 伤害时间线独立于 reanim 帧事件，攻击倍率同时推进时间线与射击轨道。
 */
class GloomShroom : public Shroom
{
public:
	using Shroom::Shroom;

	void PlantUpdate() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

	bool IsAttacking() const { return mAttacking; }
	float GetShootTimer() const { return mShootTimer; }
	float GetAttackElapsed() const { return mAttackElapsed; }
	int GetNextCloudIndex() const { return mNextCloudIndex; }
	int GetNextDamageIndex() const { return mNextDamageIndex; }
	/** AutoTest 专用：把空闲攻击周期推进到指定秒数，并清理未完成攻击。 */
	void SetShootCycleForTesting(float elapsedSeconds);

private:
	bool HasTargetInRange() const;
	bool IsTargetInRange(Zombie* zombie) const;
	void StartAttack(float attackSpeedMultiplier);
	void AdvanceAttack(float scaledDeltaTime);
	void EmitCloud() const;
	void ApplyDamagePulse() const;

	float mShootTimer = 0.0f;
	float mAttackElapsed = 0.0f;
	int mNextCloudIndex = 0;
	int mNextDamageIndex = 0;
	bool mAttacking = false;
};

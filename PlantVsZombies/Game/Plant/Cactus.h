#pragma once

#include "Plant.h"

/** 发射帧伤尖刺，并按气球目标在高、低姿态间切换的经典仙人掌。 */
class Cactus final : public Plant
{
public:
	using Plant::Plant;

	enum class Phase {
		LOW,
		RISING,
		HIGH,
		LOWERING,
	};

	/** 保存姿态、索敌缓存与攻击计时器，保持读档前后的状态机连续。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复姿态与计时器；旧档缺字段时兼容为低姿态。 */
	void LoadExtraData(const nlohmann::json& j) override;
	/** 推进空中目标索敌、伸缩演出和对应高度层射击。 */
	void PlantUpdate() override;
	/** 仙人掌能感知空中和地面两层，实际优先级由姿态状态机决定。 */
	bool CanAcquireZombie(const Zombie* zombie) const override;

	Phase GetPhase() const { return mPhase; }
	const char* GetPhaseName() const;
	bool HasCachedGroundTarget() const { return mHasGroundTarget; }
	bool HasCachedFlyingTarget() const { return mHasFlyingTarget; }

protected:
	/** 注册主人指定的第 26 帧低姿态与第 70 帧高姿态发射事件。 */
	void SetupPlant() override;

private:
	float mCheckZombieTimer = 0.0f;
	float mAttackCheckTimer = 0.0f;
	float mShootTimer = 1.0f;
	Phase mPhase = Phase::LOW;
	bool mHasGroundTarget = false;
	bool mHasFlyingTarget = false;

	/** 节流扫描本行并分别缓存可命中的地面与空中目标。 */
	void UpdateTargetCache();
	/** 从对应姿态的稳定视觉锚点发射一枚限定高度层的帧伤尖刺。 */
	void ShootSpike(bool targetsFlying);
};

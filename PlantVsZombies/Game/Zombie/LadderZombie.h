#pragma once

#include "Zombie.h"

class Plant;

/**
 * @brief 经典扶梯僵尸：携梯高速前进，为坚果类放梯，失去扶梯后按普通僵尸行动。
 */
class LadderZombie final : public Zombie {
public:
	using Zombie::Zombie;

	enum class Phase {
		CARRYING,
		PLACING,
		NORMAL,
	};

	Phase GetPhase() const { return mPhase; }
	const char* GetPhaseName() const;
	ArmorBrokenState GetShieldStage() const { return mShieldStage; }
	int GetPlacementRow() const { return mPlacementRow; }
	int GetPlacementColumn() const { return mPlacementColumn; }

	void StartEat(ColliderComponent* other) override;
	void ShieldDrop() override;
	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;
	bool HasMagneticItem() const override;
	bool ExtractMagneticItem(MagneticItem& item) override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	void ZombieUpdate(float scaledTime) override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	void PlayWalkAnimation(float blendTime) override;
	void CheckShieldImage() override;

private:
	/** 判断目标是否是当前 C# Ladder 攻击类型允许的坚果/高坚果/南瓜。 */
	bool IsLadderTarget(const Plant* plant) const;
	/** 在保存的格子重新取得当前合法放梯目标。 */
	Plant* ResolvePlacementTarget() const;
	/** 停止移动并开始一次性放梯动画。 */
	void BeginPlacement(Plant& plant);
	/** 结束携梯护盾并恢复普通走路/啃食；可选择生成掉梯粒子。 */
	void DetachLadder(bool emitParticle);
	/** 按当前护盾阶段恢复扶梯贴图；供受击与读档共用。 */
	void ApplyShieldImage() const;
	/** 恢复断臂终态，避免读档或换轨后部件复活。 */
	void ApplyBrokenArmPresentation() const;
	/** 把 C# 每 tick 水平速度换算为当前 12 FPS 资源的 clip 倍率。 */
	static float WalkClipFromVelocity(float velocity);

	Phase mPhase = Phase::CARRYING;
	ArmorBrokenState mShieldStage = ArmorBrokenState::NO_BROKEN;
	int mPlacementRow = -1;
	int mPlacementColumn = -1;
	float mWalkVelocity = 0.8f;
};

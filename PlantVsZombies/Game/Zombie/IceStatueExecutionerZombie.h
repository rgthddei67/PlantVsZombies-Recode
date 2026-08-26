#pragma once

#include "Zombie.h"

class Plant;

/**
 * 冰像处刑者：在冻土上封存一株高价值植物，以三次可延缓但不可回滚的锤击完成处决。
 * 红色橄榄球头盔是 anim_head1 的跟随贴图，耐久与掉落状态独立；扶梯轨本身始终隐藏。
 */
class IceStatueExecutionerZombie final : public Zombie {
public:
	using Zombie::Zombie;

	enum class ExecutionPhase {
		READY,
		EXECUTING,
		SPENT,
	};

	void StartEat(ColliderComponent* other) override;
	void Die() override;
	void HeadDrop() override;
	void ArmDrop() override;
	void HelmDrop() override;
	bool HasMagneticItem() const override;
	bool ExtractMagneticItem(MagneticItem& item) override;
	float GetInterruptibleSpecialActionRemaining() const override;
	bool InterruptUncommittedSpecialAction() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	void ZombieItemUpdate() const override;

	ExecutionPhase GetExecutionPhase() const { return mExecutionPhase; }
	int GetExecutionTargetPlantID() const { return mExecutionTargetPlantID; }
	int GetExecutionProgress() const { return mExecutionProgress; }
	bool HasUsedExecution() const { return mExecutionUsed; }
	ArmorBrokenState GetHelmetStage() const { return mHelmetStage; }
	bool HasHelmetFollower() const;
	bool OwnsIceSealFor(int plantID) const;
	/** 全实体恢复后校验植物侧拥有者、冻土和终止态，不重放音画。 */
	void FinalizeIceSealLoad();
	/** AutoTest 专用：在保持双向关系的前提下进入指定锤击状态。 */
	bool SetExecutionStateForTesting(Plant* target, int progress);

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	void ZombieMove(float scaledDelta, Transform* transform) override;
	void ZombieUpdate(float scaledTime) override;
	void PlayWalkAnimation(float blendTime) override;
	void OnStartEating() override;
	void OnMindControlled() override;
	void CheckHelmImage() override;

private:
	bool IsFullyOnBattlefield() const;
	bool CanOwnExecution() const;
	bool BeginExecution(Plant& target);
	void BeginStrike(float blendTime);
	void CommitStrike();
	void AbortExecution(bool consumeAbility, bool restoreWalkAnimation);
	Plant* ResolveExecutionTarget() const;
	void ConfigureHelmetFollower();
	void SyncHelmetPresentation() const;
	void HideHelmetFollower() const;
	void ApplyExecutionerTextures() const;
	const std::string& GetHelmetTextureKey() const;
	static float WalkClipFromVelocity(float velocity);

	ExecutionPhase mExecutionPhase = ExecutionPhase::READY;
	int mExecutionTargetPlantID = NULL_PLANT_ID;
	int mExecutionProgress = 0;
	bool mExecutionUsed = false;
	float mWalkVelocity = 0.30f;
	ArmorBrokenState mHelmetStage = ArmorBrokenState::NO_BROKEN;
	mutable bool mHelmetFollowerConfigured = false;
};

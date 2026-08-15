#pragma once

#include "Zombie.h"

/** 经典巨人僵尸：接触目标时砸击，并在半血后把唯一一只小鬼投向前方。 */
class GargantuarZombie : public Zombie {
public:
	using Zombie::Zombie;

	enum class Phase {
		WALKING,
		SMASHING,
		THROWING,
	};
	enum class WeaponVariant {
		TELEPHONE_POLE,
		DUCK_SIGN,
		ZOMBIE,
	};

	void Update() override;
	void ZombieUpdate(float scaledTime) override;
	void TakeBodyDamage(int damage) override;
	/** 巨人拒绝被大嘴花吞食；攻击者随后按统一规则结算小额咬伤。 */
	bool TakePlantInstantKill() override;
	void StartEat(ColliderComponent* other) override;
	void Charred() override;
	void ZombieItemUpdate() const override;
	bool CanBeGrabbedByTangleKelp() const override { return false; }
	float GetButterSplatScaleMultiplier() const override;
	bool ShouldDrawButterSplatAfterAllTracks() const override { return false; }
	float GetIceTrapScaleMultiplier() const override;

	Phase GetPhase() const { return mPhase; }
	bool HasImp() const { return mHasImp; }
	bool HasAppliedSmash() const { return mSmashApplied; }
	bool HasReleasedImp() const { return mThrowReleased; }
	int GetActionTargetRow() const { return mTargetRow; }
	int GetActionTargetColumn() const { return mTargetColumn; }
	int GetActionTargetZombieID() const { return mTargetZombieID; }
	float GetThrowDistance() const { return mThrowDistance; }
	WeaponVariant GetWeaponVariant() const { return mWeaponVariant; }
	int GetDamageStage() const;
	/** 返回当前伤势阶段实际应使用的头部贴图键，供表现重建与测试取证共用。 */
	const std::string& GetCurrentHeadTextureKey() const;

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	void PlayWalkAnimation(float blendTime) override;
	float GetAbilityAnimSpeedMultiplier() const override;
	void OnMindControlled() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	/** 按伤势阶段选择头部材质；同时间线换色变体只需覆写此入口。 */
	virtual const std::string& GetHeadTextureKey(int damageStage) const;
	/** 按生命比例恢复身体、手臂、脚和头部的两档受伤换图。 */
	void ApplyDamagePresentation() const;

private:
	/** 从行走态进入一次砸击，并冻结本次植物格或敌对僵尸目标。 */
	void BeginSmash(int row, int column, int zombieID);
	/** 主人指定的第 93 帧回调：压扁目标格植物或重击敌对僵尸。 */
	void ApplySmashImpact();
	/** 半血且位置允许时开始唯一一次投掷。 */
	void TryBeginThrow();
	/** 主人确认的第 131 帧回调：生成小鬼、继承阵营与剩余减速。 */
	void ReleaseImp();
	/** 原子清理移动抑制动作；死亡入口可保留随后接管的死亡轨。 */
	void AbortAction(bool playWalkingTrack);
	/** 按唯一权威 mHasImp 同步巨人本体内嵌的小鬼与白绳轨道。 */
	void ApplyHeldImpPresentation() const;
	/** 按出生时冻结的随机结果恢复电线杆、鸭子路牌或普通僵尸持物。 */
	void ApplyWeaponPresentation() const;
	void PlayWalking(float blendTime = 0.0f);

	Phase mPhase = Phase::WALKING;
	bool mHasImp = true;
	bool mSmashApplied = false;
	bool mThrowReleased = false;
	bool mDeathSoundPlayed = false;
	int mTargetRow = -1;
	int mTargetColumn = -1;
	int mTargetZombieID = NULL_ZOMBIE_ID;
	float mThrowDistance = 0.0f;
	float mAnimSpeedMultiplier = 0.5f; // 每只巨人出生时独立抽取并随关卡存档的整体动画倍率
	WeaponVariant mWeaponVariant = WeaponVariant::TELEPHONE_POLE;
};

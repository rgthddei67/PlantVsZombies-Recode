#pragma once

#include "Zombie.h"

/**
 * @brief 极光祭司僵尸：在逻辑计时仪式后向玩家阵线提交独立极光裂隙。
 *
 * 本体完整复用普通僵尸时间线；极光仪器和胸前光谱片使用命名 follower。
 */
class AuroraPriestZombie final : public Zombie {
public:
	using Zombie::Zombie;

	enum class RitualPhase {
		PREPARING,
		WINDUP,
		RETRY_WAIT,
		COMMITTED,
		DISABLED,
	};

	void Update() override;
	void Die() override;
	void HeadDrop() override;
	void HelmDrop() override;
	void ZombieItemUpdate() const override;
	void OnTemporalCoreStateRestored() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	float GetInterruptibleSpecialActionRemaining() const override;
	bool InterruptUncommittedSpecialAction() override;
	bool HasCommittedIrreversibleSpecialAction() const override {
		return mRitualPhase == RitualPhase::COMMITTED;
	}
	void RestoreCommittedIrreversibleSpecialAction(bool submitted) override;
	bool CaptureTemporalAbilityState(ZombieTemporalAbilityState& state) const override;
	void RestoreTemporalAbilityState(const ZombieTemporalAbilityState& state) override;

	RitualPhase GetRitualPhase() const { return mRitualPhase; }
	float GetRitualRemaining() const { return mRitualRemaining; }
	bool IsOverloaded() const { return mOverloaded; }
	bool HasFinaleFollowersConfigured() const { return mFollowersConfigured; }

protected:
	void SetupZombie() override;
	void ZombieMove(float scaledDelta, Transform* transform) override;
	void OnMindControlled() override;
	float GetAbilityAnimSpeedMultiplier() const override {
		return mOverloaded ? 1.15f : 0.65f;
	}

private:
	/** 配置不新增时间轴的极光仪器与光谱片 follower。 */
	void ConfigureFollowers();
	/** 从准备/重试边沿抢占啃食并进入完整仪式前摇。 */
	void BeginWindup();
	/** 永久取消尚未提交的仪式，同时保留已经提交的 Board 裂隙。 */
	void DisableUncommittedRitual();
	/** 按当前防具、断头和死亡状态同步附加分件。 */
	void SyncFollowerPresentation() const;

	RitualPhase mRitualPhase = RitualPhase::PREPARING;
	float mRitualRemaining = 6.0f;
	float mRitualVisualPulseTimer = 0.0f;
	bool mOverloaded = false;
	bool mFollowersConfigured = false;
};

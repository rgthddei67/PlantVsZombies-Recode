#pragma once

#include "ConeZombie.h"

/**
 * 冰墙工程师：到达冬日花园霜线后进行可中断施工，完工后留下与自身生命独立的唯一冰墙。
 * 行走、啃食、断肢断头和死亡事件全部复用路障僵尸时间轴。
 */
class IceWallEngineerZombie final : public ConeZombie {
public:
	using ConeZombie::ConeZombie;

	enum class ConstructionPhase {
		MOVING,
		BUILDING,
		COMPLETED,
	};

	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	void ZombieItemUpdate() const override;

	ConstructionPhase GetConstructionPhase() const { return mConstructionPhase; }
	float GetConstructionRemaining() const { return mConstructionRemaining; }
	float GetBuildWallCenterX() const { return mBuildWallCenterX; }
	bool HasUsedConstruction() const { return mConstructionUsed; }
	/** AutoTest 专用：直接进入指定施工状态，不提交冰墙。 */
	void SetConstructionStateForTesting(
		ConstructionPhase phase, float remaining, float wallCenterX, bool used);

protected:
	void SetupZombie() override;
	void ZombieMove(float scaledDelta, Transform* transform) override;
	void ZombieUpdate(float scaledTime) override;
	void OnMindControlled() override;
	const std::string& GetConeTextureKey(ArmorBrokenState stage) const override;
	const char* GetConeDropEffectName() const override {
		return "IceWallEngineerHardhatOff";
	}

private:
	bool CanBeginConstruction() const;
	bool ShouldAbortConstruction() const;
	void BeginConstruction(float frontierX);
	void CancelConstruction(bool consumeAbility);
	void CompleteConstruction();
	void ApplyEngineerEquipmentTextures() const;

	ConstructionPhase mConstructionPhase = ConstructionPhase::MOVING;
	float mConstructionRemaining = 0.0f;
	float mBuildWallCenterX = 0.0f;
	float mBuildParticleTimer = 0.0f;
	bool mConstructionUsed = false;
};

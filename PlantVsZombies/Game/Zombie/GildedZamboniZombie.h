#pragma once

#include "ZamboniZombie.h"

/**
 * @brief 鎏金冰车僵尸：铺设三路黄色冰道，并在持续无伤时阶梯加速。
 */
class GildedZamboniZombie final : public ZamboniZombie {
public:
	using ZamboniZombie::ZamboniZombie;

	void Update() override;
	void TakeBodyDamage(int damage) override;
	bool TakePlantInstantKill() override;
	void HandleCaltropHit(Caltrop& caltrop) override;
	void SetCooldown(float timer) override { Zombie::SetCooldown(timer); }

	bool CanBeChilled() const override { return Zombie::CanBeChilled(); }
	bool CanBeFrozen() const override { return true; }

	float GetUndamagedTime() const { return mUndamagedTime; }
	int GetAccelerationStage() const { return mAccelerationStage; }
	float GetAccelerationMultiplier() const;
	/** @brief 判断指定点是否由本车仍存活的黄色冰道来源覆盖，供多车速度场叠层。 */
	bool ProvidesGoldenIceEffectAt(int row, float worldX, bool includeVehicleBody) const;

protected:
	void SetupZombie() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	void LayIceTrails(const Vector& stableVisualOrigin) override;
	bool CanCrushRow(int row) const override;
	const char* GetDamageTexturePrefix() const override {
		return "IMAGE_ZOMBIE_GILDED_ZAMBONI_";
	}
	float GetBaseDriveSpeedMultiplier() const override;
	float GetAbilityAnimSpeedMultiplier() const override;
	bool IsAlwaysAffectedByGoldenIce() const override { return true; }
	const char* GetDeathParticleEffectName() const override {
		return "GildedZamboniExplosion";
	}

private:
	/** @brief 依据无伤害时间推进 6/10/14 秒三个加速门槛。 */
	void UpdateAcceleration(float deltaTime);
	/** @brief 任何实际本体承伤都清空计时和全部加速层。 */
	void ResetAcceleration();

	float mUndamagedTime = 0.0f;
	int mAccelerationStage = 0;
	float mGoldenTrailMinX = 0.0f; // 本车独立黄色冰道最左缘，单位 px；Setup 后以 Board 右界为单位元
};

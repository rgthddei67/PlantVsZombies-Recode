#pragma once

#include "Zombie.h"

/**
 * @brief 经典冰车僵尸：碾压植物、铺设冰道，并使用车辆专属损坏与死亡表现。
 */
class ZamboniZombie final : public Zombie {
public:
	using Zombie::Zombie;

	void ZombieUpdate(float scaledTime) override;
	void TakeBodyDamage(int damage) override;
	void ZombieItemUpdate() const override;
	void Die() override;
	void Charred() override;
	void StartEat(ColliderComponent* other) override;
	void SetCooldown(float timer) override {}

	bool CanBeCharmed() const override { return false; }
	bool CanBeChilled() const override { return false; }
	bool CanBeFrozen() const override { return false; }
	bool CanBeGrabbedByTangleKelp() const override { return false; }

	int GetDamageStage() const;
	float GetDriveSpeed() const { return mDriveSpeed; }
	/** 返回当前地图供冰车速度曲线使用的水平坐标基准。 */
	float GetDriveCoordinateBaseX() const;
	Vector GetDamageShakeOffset() const { return mDamageShakeOffset; }
	Vector GetVisualPosition() const override;

protected:
	void SetupZombie() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

private:
	/** @brief 按当前血量恢复两段车辆损坏贴图。 */
	void ApplyDamageVisuals() const;
	/** @brief 检查同排车辆攻击矩形并压扁允许被碾过的植物。 */
	void CrushPlants();
	/** @brief 判断植物是否属于原版不可被冰车直接碾压的例外。 */
	bool CanCrushPlant(const Plant* plant) const;

	float mDriveSpeed = 25.0f;
	float mSmokeTimer = 0.0f;
	Vector mDamageShakeOffset;
	bool mSuppressDeathEffects = false;
	bool mDeathEffectsEmitted = false;
};

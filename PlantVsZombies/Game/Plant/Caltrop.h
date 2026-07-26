#pragma once

#include "Plant.h"

/**
 * @brief 经典地刺：周期性伤害脚下的同排僵尸，且不会成为普通啃食目标。
 */
class Caltrop : public Plant {
public:
	using Plant::Plant;

	void PlantUpdate() override;
	bool CanBeEaten() const override { return false; }

	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

	float GetAttackCooldown() const { return mAttackCooldown; }

protected:
	void SetupPlant() override;

private:
	/** @brief 检查原版窄攻击带内是否存在可伤害目标。 */
	bool HasTargetInAttackRect() const;
	/** @brief 在主人指定的第 25 帧结算当前攻击带内的全部目标。 */
	void DamageTargetsAtAttackFrame();
	/** @brief 起播一次攻击并重置原版约一秒的攻击周期。 */
	void StartAttack();

	float mAttackCooldown = 0.0f;
};

#pragma once

#include "DiggerZombie.h"

/**
 * @brief 爆破工头矿工：完整出土后爆破房屋侧三列，再持镐向右折返。
 */
class EliteDiggerZombie final : public DiggerZombie {
public:
	using DiggerZombie::DiggerZombie;

	bool HasResolvedBlast() const { return mBlastResolved; }
	/** 爆破工头的镐子承载核心预警能力，主人指定免疫磁力菇。 */
	bool HasMagneticItem() const override { return false; }

protected:
	void SetupZombie() override;
	void OnPickaxeStunFinished() override;
	void OnPickaxeLost(Phase previousPhase) override;
	float GetPickaxeWalkVelocity() const override;
	const std::string& GetFullHardhatTexture() const override;
	const std::string& GetDamagedHardhatTexture(bool heavilyDamaged) const override;
	const std::string& GetBrokenOuterArmTexture() const override;
	const char* GetHelmDropEffectName() const override;
	const char* GetArmDropEffectName() const override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

private:
	/** 对固定逻辑格结算一次伤害，并发射与 footprint 对齐的独立特效。 */
	void ResolveBlast();

	bool mBlastResolved = false;
};

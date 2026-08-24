#pragma once

#include "WallNut.h"

/**
 * @brief 雪锚果：所在格冻结时可消费一次锚定，整形雪橇碰撞或冻土地裂。
 */
class SnowAnchorNut final : public WallNut {
public:
	using WallNut::WallNut;

	void PlantUpdate() override;
	bool IsWinterGroundAnchorReady() const override;
	bool HasSpentWinterGroundAnchor() const override { return mBraceSpent; }
	WinterGroundImpactResponse ResolveWinterGroundImpact(
		WinterGroundImpactKind kind) override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

protected:
	void SetupPlant() override;
	const std::string& GetBodyTextureKey() const override;
	const std::string& GetCrackedTextureKey(int damageStage) const override;

private:
	bool mBraceSpent = false;
	bool mLastBraceReady = false; // 仅用于检测派生外观边沿，不进入存档

	/** 冻土资格或消费状态变化后，立即重建当前生命阶段材质。 */
	void RefreshBracePresentation();
};

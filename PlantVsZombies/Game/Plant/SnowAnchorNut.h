#pragma once

#include "WallNut.h"

/**
 * @brief 雪锚果：所在格冻结且仍存活时持续锚定，整形雪橇碰撞或冻土地裂。
 */
class SnowAnchorNut final : public WallNut {
public:
	using WallNut::WallNut;

	void PlantUpdate() override;
	bool IsWinterGroundAnchorReady() const override;
	WinterGroundImpactResponse ResolveWinterGroundImpact(
		WinterGroundImpactKind kind) override;

protected:
	void SetupPlant() override;
	const std::string& GetBodyTextureKey() const override;
	const std::string& GetCrackedTextureKey(int damageStage) const override;

private:
	bool mLastBraceReady = false; // 仅用于检测派生外观边沿，不进入存档

	/** 冻土资格变化后，立即重建当前生命阶段材质。 */
	void RefreshBracePresentation();
};

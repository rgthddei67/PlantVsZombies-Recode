#pragma once

#include "Plant.h"

/** @brief 冰镜草：持续凝结至多两面镜片，逐面吸收敌方水平直射弹。 */
class IceMirrorGrass final : public Plant {
public:
	using Plant::Plant;

	void PlantUpdate() override;
	void Draw(Graphics* g) override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	bool TryInterceptHostileStraightProjectile(
		float velocityX, const Vector& impactPosition) override;

	int GetMirrorCount() const { return mMirrorCount; }
	float GetFormationProgress() const { return mFormationProgress; }

protected:
	void SetupPlant() override;

private:
	int mMirrorCount = 0;
	float mFormationProgress = 0.0f;
};

#pragma once

#include "Plant.h"
#include "../WeatherTypes.h"

/** 经典模仿者占位实体；目标的落种语义由 Board 在创建时代理。 */
class Imitater final : public Plant {
public:
	using Plant::Plant;

	/** 设置被模仿的已注册植物；非法目标会清空配置。 */
	void SetImitaterTarget(PlantType target);
	PlantType GetImitaterTarget() const { return mImitaterTarget; }
	bool HasValidTarget() const;
	PlantType GetPlacementType() const override { return mImitaterTarget; }
	float GetMorphCountdown() const { return mMorphCountdown; }
	bool IsMorphing() const { return mMorphing; }
	bool HasEmittedMorphParticle() const { return mMorphParticleEmitted; }

	void SetInheritedBloverDirection(WindDirection direction);
	WindDirection GetInheritedBloverDirection() const { return mInheritedBloverDirection; }

	void PlantUpdate() override;
	void Die() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

protected:
	void SetupPlant() override;

private:
	PlantType mImitaterTarget = PlantType::NUM_PLANT_TYPES;
	float mMorphCountdown = 2.0f;
	bool mMorphing = false;
	bool mMorphParticleEmitted = false;
	WindDirection mInheritedBloverDirection = WindDirection::TOWARD_FRONT;

	void BeginMorph();
	void EmitMorphParticle();
};

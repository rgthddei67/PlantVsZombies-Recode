#pragma once

#include "Plant.h"
#include "../WeatherTypes.h"

/**
 * @brief 三叶草：第 44 帧按卡槽方向吹风，第 61 帧演出结束后消失。
 */
class Blover final : public Plant {
public:
	using Plant::Plant;
	/** 已进入结算动作时禁止搬运，保留原目标及动作提交位置。 */
	bool CanBeRelocated() const override { return false; }

	void SetBlowDirection(WindDirection direction);
	WindDirection GetBlowDirection() const { return mBlowDirection; }
	bool HasTriggeredBlow() const { return mBlowTriggered; }

protected:
	void SetupPlant() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

private:
	void TriggerBlow();
	void ApplyDirectionPresentation();

	WindDirection mBlowDirection = WindDirection::TOWARD_FRONT;
	bool mBlowTriggered = false;
};

#pragma once

#include "Plant.h"

enum class PlanternGear : int {
	OFF = 0,
	LOW = 1,
	MEDIUM = 2,
	HIGH = 3,
};

/**
 * 四大关迷雾的唯一照明核心。
 *
 * 燃料、在途预留量与挡位属于实体状态并进入关卡存档；卡槽菜单和满仓闪烁不影响玩法。
 */
class Plantern : public Plant {
public:
	static constexpr float FUEL_CAPACITY = 100.0f;
	static constexpr float INITIAL_FUEL = 30.0f;

	using Plant::Plant;

	void PlantUpdate() override;
	void Draw(Graphics* g) override;
	void Die() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

	/** 尝试加入雾火，返回实际接收量；溢出部分直接丢弃并触发卡槽满仓提示。 */
	float AddFuel(float amount);
	/** 为飞行雾火预留容量；同批在途量受当前单波预算限制，避免跨波奖励被瞬间兑现。 */
	float ReserveFuel(float amount);
	/** 雾火飞抵本体后，把对应预留量正式计入燃料。 */
	void DeliverReservedFuel(float amount);
	void SetFuel(float fuel);
	void SetGear(PlanternGear gear);

	float GetFuel() const { return mFuel; }
	float GetFuelRatio() const { return mFuel / FUEL_CAPACITY; }
	float GetPendingFuel() const { return mPendingFuel; }
	/** 返回当前挡位在当前波次的每游戏秒燃料消耗。 */
	float GetCurrentBurnRate() const;
	PlanternGear GetGear() const { return mGear; }
	bool HasUsableLight() const {
		return !mIsPreview && !IsSquished()
			&& mGear != PlanternGear::OFF && mFuel > 0.0f;
	}
	float GetFuelFullHintTimer() const { return mFuelFullHintTimer; }

protected:
	void SetupPlant() override;

private:
	float mFuel = INITIAL_FUEL;
	float mPendingFuel = 0.0f;
	PlanternGear mGear = PlanternGear::LOW;
	float mFuelFullHintTimer = 0.0f;

	float GetBurnRate(PlanternGear gear) const;
};

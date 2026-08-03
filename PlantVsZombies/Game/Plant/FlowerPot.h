#pragma once

#include "Plant.h"

/**
 * 经典花盆植物本体。
 *
 * 本类负责原版种下后一秒的啃咬保护、覆盖动画状态与独立阴影校准；承载层和种植门禁由 Board 统一负责。
 */
class FlowerPot : public Plant {
private:
	float mBiteProtectionTimer = 1.0f;	// 原版 FlowerpotInvulnerable 的 100cs，仅阻止僵尸啃咬
	bool mCovered = false;	// 同格存在上层植物时冻结花盆待机动画，由 Cell 当前状态派生

public:
	using Plant::Plant;

	/** 推进种下后的短暂无啃食时间。 */
	void PlantUpdate() override;
	/** 保护倒计时结束后才允许僵尸把花盆选为啃食目标。 */
	bool CanBeEaten() const override;
	bool IsBiteProtected() const { return mBiteProtectionTimer > 0.0f; }
	bool IsCovered() const { return mCovered; }

	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

	void Draw(Graphics* g) override;

protected:
	/** 使用现有 ShadowComponent 复现原版花盆相对通用阴影的偏移。 */
	void SetupPlant() override;
};

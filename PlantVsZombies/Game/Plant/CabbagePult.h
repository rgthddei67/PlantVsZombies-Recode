#pragma once

#include "Plant.h"

/**
 * 经典卷心菜投手：按当前行目标的预计落点发射解析抛物线卷心菜。
 */
class CabbagePult final : public Plant
{
public:
	using Plant::Plant;

	void PlantUpdate() override;
	/** 保存当前攻击周期，保证读档后发射相位连续。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复攻击周期；旧档采用中性首轮状态。 */
	void LoadExtraData(const nlohmann::json& j) override;

	float GetShootTimer() const { return mShootTimer; }
	float GetShootInterval() const { return mShootInterval; }
	/** AutoTest 专用：布置一个将到期的正式攻击周期。 */
	void SetShootCycleForTesting(float elapsedSeconds, float intervalSeconds);

protected:
	/** 设置待机、随机首轮相位和主人确认的第 43 帧发射事件。 */
	void SetupPlant() override;

private:
	float mShootTimer = 0.0f;
	float mShootInterval = 3.0f;

	/** 返回同行最近的合法地面目标；未找到时返回 nullptr。 */
	class Zombie* FindTarget() const;
	/** 在动画发射帧重新预测落点并创建卷心菜。 */
	void FireCabbage();
};

#pragma once

#include "Plant.h"

enum class BulletType;

/**
 * 经典西瓜投手：预判同行目标落点，投出会溅射相邻行的西瓜。
 */
class MelonPult : public Plant
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
	/** 设置待机、随机首轮相位和主人确认的第 44 帧发射事件。 */
	void SetupPlant() override;
	/** 返回该西瓜投手品种创建的投射物类型。 */
	virtual BulletType GetMelonBulletType() const;

private:
	float mShootTimer = 0.0f;
	float mShootInterval = 3.0f;

	/** 返回同行最近的合法地面目标；未找到时返回 nullptr。 */
	class Zombie* FindTarget() const;
	/** 在动画发射帧重新预测落点并创建西瓜。 */
	void FireMelon();
};

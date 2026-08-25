#pragma once

#include "Plant.h"

/**
 * 经典玉米投手：优先攻击同行冰墙，否则按 3:1 概率投掷玉米粒或黄油。
 */
class KernelPult final : public Plant
{
public:
	using Plant::Plant;

	void PlantUpdate() override;
	/** 保存攻击周期与当前射击动画手持的弹型。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复攻击周期与手持弹型；旧档采用普通玉米粒。 */
	void LoadExtraData(const nlohmann::json& j) override;

	float GetShootTimer() const { return mShootTimer; }
	float GetShootInterval() const { return mShootInterval; }
	bool IsButterShotPending() const { return mButterShotPending; }
	/** AutoTest 专用：布置一个将到期的周期，并可固定下一发弹型。 */
	void SetShootCycleForTesting(
		float elapsedSeconds, float intervalSeconds, int forcedShot = -1);

protected:
	/** 设置待机、手持弹型轨道和主人确认的第 30 帧发射事件。 */
	void SetupPlant() override;

private:
	float mShootTimer = 0.0f;
	float mShootInterval = 3.0f;
	bool mButterShotPending = false;
	int mForcedShotForTesting = -1;

	/** 返回同行最近的合法地面目标；未找到时返回 nullptr。 */
	class Zombie* FindTarget() const;
	/** 同步玉米粒与黄油手持轨道；读档恢复也复用该入口。 */
	void ApplyHeldProjectileVisual();
	/** 在攻击开始时决定弹型并切换手持轨道。 */
	void BeginShot();
	/** 在动画发射帧重新预测落点，创建本轮玉米粒或黄油。 */
	void FireProjectile();
};

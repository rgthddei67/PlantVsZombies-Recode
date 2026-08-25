#pragma once

#include "Plant.h"

/**
 * 融雪投手：所有弹型优先攻击同行冰墙；准确寒潮预报装填三发盐晶弹。
 */
class MeltSnowPult final : public Plant
{
public:
	using Plant::Plant;

	void PlantUpdate() override;
	void OnColdWaveForecastDisrupted() override;
	/** 保存攻击相位、盐晶库存和动画中已经选定的弹型。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复攻击与装填状态；旧档从无库存的普通雪团状态开始。 */
	void LoadExtraData(const nlohmann::json& j) override;

	float GetShootTimer() const { return mShootTimer; }
	float GetShootInterval() const { return mShootInterval; }
	int GetSaltAmmo() const { return mSaltAmmo; }
	bool IsSaltShotPending() const { return mSaltShotPending; }
	bool HasObservedColdWaveForecast() const { return mObservedColdWaveForecast; }
	/** AutoTest 专用：布置攻击周期，并可固定下一发为普通弹或盐晶弹。 */
	void SetShootCycleForTesting(
		float elapsedSeconds, float intervalSeconds, int forcedShot = -1);
	/** AutoTest 专用：固定可持久化盐晶状态，不直接触发射击或预报。 */
	void SetSaltStateForTesting(int ammo, bool pending, bool observedForecast);

protected:
	/** 设置冰蓝投手待机、手持弹型和主人确认的第 43 帧发射事件。 */
	void SetupPlant() override;

private:
	float mShootTimer = 0.0f;
	float mShootInterval = 3.0f;
	int mSaltAmmo = 0;
	bool mSaltShotPending = false;
	bool mObservedColdWaveForecast = false;
	int mForcedShotForTesting = -1;

	/** 观察准确预报的上升沿，并把库存直接补至三发。 */
	void UpdateForecastAmmo();
	/** 返回同行最近的合法地面目标；未找到时返回 nullptr。 */
	class Zombie* FindTarget() const;
	/** 返回同行活动冰墙；普通雪团与盐晶都会优先锁定它。 */
	class IceWall* FindIceWallTarget() const;
	/** 在攻击开始时锁定并消费本发盐晶库存。 */
	void BeginShot();
	/** 同步手持雪团轨道使用的普通/盐晶纹理。 */
	void ApplyHeldProjectileVisual();
	/** 在动画发射帧重新预测落点并创建已经锁定的弹型。 */
	void FireProjectile();
};

#pragma once

#include "Plant.h"

/**
 * 经典玉米加农炮：由两株相邻玉米投手升级为一个双格实体，装填完成后由玩家指定爆心。
 */
class CobCannon final : public Plant {
public:
	using Plant::Plant;

	enum class Phase {
		ARMING,
		CHARGING,
		READY,
		FIRING,
	};

	bool IsReady() const { return mPhase == Phase::READY; }
	Phase GetPhase() const { return mPhase; }
	float GetArmingTimeRemaining() const { return mArmingTime; }
	bool HasLaunchedCurrentShot() const { return mShotLaunched; }
	const Vector& GetPendingTarget() const { return mPendingTarget; }
	int GetPendingTargetRow() const { return mPendingTargetRow; }
	/** READY 状态冻结玩家落点并进入射击轨；其他阶段返回 false。 */
	bool FireAt(const Vector& target, int targetRow);
	bool CanBeTargetedByBungee() const override { return false; }
	void Die() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	/** AutoTest 专用：缩短装填倒计时，不跳过正式状态边沿。 */
	void SetArmingTimeForTesting(float seconds) { mArmingTime = std::max(0.0f, seconds); }

protected:
	void SetupPlant() override;
	void PlantUpdate() override;

private:
	/** 开始一次 12fps 充能轨，完成后由轨道切换进入 READY。 */
	void BeginCharge();
	/** 第 78 帧创建唯一玉米棒并播放发射声。 */
	void LaunchCob();

	Phase mPhase = Phase::ARMING;
	float mArmingTime = 0.0f;
	Vector mPendingTarget = Vector::zero();
	int mPendingTargetRow = -1;
	bool mShotLaunched = false;
};

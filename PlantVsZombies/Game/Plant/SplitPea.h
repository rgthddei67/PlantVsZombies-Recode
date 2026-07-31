#pragma once

#include "Shooter.h"

class SplitPea final : public Shooter
{
public:
	using Shooter::Shooter;

	const Animator* GetRearHeadAnimator() const { return mRearHeadAnim.get(); }
	bool HasPendingRearSecondShot() const { return mRearSecondShotPending; }
	bool IsRearSecondShot() const { return mRearSecondShotInBurst; }

	/** 保存后头完整播放状态及两发之间的瞬态。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复后头一次性射击和双发瞬态，旧档缺字段时采用待机中性状态。 */
	void LoadExtraData(const nlohmann::json& j) override;

	void PlantUpdate() override;

protected:
	/** 创建并附着前后两个头，在主人指定的 95/57 帧注册独立发射事件。 */
	void SetupPlant() override;
	/** 从前头枪口发射一颗普通豌豆。 */
	void ShootBullet() override;

private:
	std::shared_ptr<Animator> mRearHeadAnim;
	bool mRearSecondShotPending = false;
	bool mRearSecondShotInBurst = false;

	/** 创建全尺寸子头并用根轨基准姿态逆变换附着到 anim_idle。 */
	std::shared_ptr<Animator> CreateHeadAnimator(const char* idleTrack);
	/** 按当前行、方向、雾可见性与目标高度寻找可攻击僵尸。 */
	bool HasTargetInDirection(bool forward) const;
	void StartForwardShot(float attackSpeedMultiplier);
	void StartRearShot(float attackSpeedMultiplier);
	/** 发射反向豌豆并维护后头两连发的两发间状态。 */
	void ShootRearBullet();
	void PlayShootSound() const;
};

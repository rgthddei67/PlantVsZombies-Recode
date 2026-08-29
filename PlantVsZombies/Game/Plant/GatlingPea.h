#pragma once

#include "Shooter.h"

/** 经典机枪射手紫卡；一次攻击动画按四个独立帧事件依次发射普通豌豆。 */
class GatlingPea final : public Shooter
{
public:
	using Shooter::Shooter;

	/** 按经典约 1.5 秒节奏启动一轮四连发动画。 */
	void PlantUpdate() override;

protected:
	/** 建立机枪射手独立头部，并注册主人给定的四个发射帧。 */
	void SetupPlant() override;
	/** 从当前炮口发射一颗保留机枪谱系的普通豌豆。 */
	void ShootBullet() override;

private:
	void PlayShotSound() const;
};

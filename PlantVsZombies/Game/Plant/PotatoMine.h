#pragma once
#ifndef _POTATO_MINE_H
#define _POTATO_MINE_H

#include "Plant.h"

class PotatoMine : public Plant {
private:
	float mReadyTimer = 0.0f;	// 准备的时间
	bool mIsBoom = false;		// 是否已经爆炸
	bool mIsRise = false;		// 是否升起

public:
	using Plant::Plant;

	void PlantUpdate() override;

	/** 武装地雷在啃咬伤害提交前立即爆炸，避免南瓜破裂后被多只僵尸同时咬死。 */
	void OnZombieBite(const Vector& eaterPosition) override;

	/** 爆炸提交后拒绝同一啃咬帧继续修改生命。 */
	void TakeDamage(int damage, DamageSource source) override;

	void SaveExtraData(nlohmann::json& j) const override;

	void LoadExtraData(const nlohmann::json& j) override;

protected:
	void SetupPlant() override;

	/** 按原版顺序结算范围伤害、粒子与震屏，然后立即释放占格并销毁本体。 */
	void Detonate();

	/** 武装后持续按原版目标资格扫描爆区，补足已有碰撞对不会再触发 enter 的路径。 */
	bool HasTriggeringZombieInBlastRadius();

	/** 清除爆炸圆与僵尸判定矩形相交的所有非魅惑目标。 */
	void KillZombiesInBlastRadius();

	void Ready(bool quick);
};

#endif

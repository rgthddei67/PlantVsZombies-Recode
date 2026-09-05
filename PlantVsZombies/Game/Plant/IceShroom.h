#pragma once
#ifndef _H_ICESHROOM_H
#define _H_ICESHROOM_H

#include "Shroom.h"
#include "Game/Board/Board.h"

// 寒冰菇：种下后 anim_idle 第 16 帧（主人指定）一次性全场冻结——
// 每个僵尸 20 点伤害 + 完全定身（首冻 4~6s）+ 20s 减速尾巴，随即本体消失。
// 白天种下睡觉（anim_sleep 活跃区间 17..33，不经过第 16 帧，天然不触发）。
class IceShroom : public Shroom
{
public:
	using Shroom::Shroom;
	/** 动作提交期间不搬运，防止原目标和结算位置失配。 */
	bool CanBeRelocated() const override { return Plant::CanBeRelocated() && GetSleepState() && !IsWakingUp(); }

	void TakeDamage(int damage, DamageSource source) override;
	void TakeDeploymentInterceptionDamage(int damage, DamageSource source) override {
		Plant::TakeDamage(damage, source);
	}
	/** 清醒引爆中的寒冰菇被巨人锤击时立即冻结全场；睡眠态仍走普通压扁。 */
	void ResolveGargantuarSmash() override;

protected:
	void SetupPlant() override;

private:
	/** 统一执行自然到帧与巨人锤击触发的完整冻结结算。 */
	void Freeze();
	void FreezeAllZombies();
};

#endif

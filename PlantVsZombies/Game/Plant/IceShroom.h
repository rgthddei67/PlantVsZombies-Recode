#pragma once
#ifndef _H_ICESHROOM_H
#define _H_ICESHROOM_H

#include "Shroom.h"
#include "../Board.h"

// 寒冰菇：种下后 anim_idle 第 16 帧（主人指定）一次性全场冻结——
// 每个僵尸 20 点伤害 + 完全定身（首冻 4~6s）+ 20s 减速尾巴，随即本体消失。
// 白天种下睡觉（anim_sleep 活跃区间 17..33，不经过第 16 帧，天然不触发）。
class IceShroom : public Shroom
{
public:
	using Shroom::Shroom;

	void TakeDamage(int damage, DamageSource source) override;

protected:
	void SetupPlant() override;

private:
	void FreezeAllZombies();
};

#endif

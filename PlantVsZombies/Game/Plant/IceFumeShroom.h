#pragma once
#ifndef _ICE_FUME_SHROOM_H
#define _ICE_FUME_SHROOM_H

#include "FumeShroom.h"

// 寒冰大喷菇（自创变种）：大喷菇的寒冰版。
// 本体蓝色覆盖（与僵尸减速同色），射程比大喷菇短 30，孢子云为蓝色；
// 每 2.5s 喷一次，对锥形内僵尸 10 点穿透伤害 + 2.0s 减速（持盾不吃减速=SetCooldown 守卫，
// 与寒冰射手同语义）。减速时长 < 攻击间隔 = 刻意的恢复空窗，防整行永久减速超模。
class IceFumeShroom : public FumeShroom {
protected:
	void SetupPlant() override;
	const char* FumeParticleName() const override { return "IceFumeCloud"; }
	void OnFumeHit(Zombie* zombie) override;

public:
	using FumeShroom::FumeShroom;
};

#endif

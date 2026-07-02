#pragma once
#ifndef _HYPNOSHROOM_H_
#define _HYPNOSHROOM_H_

#include "Shroom.h"

// 魅惑菇：无攻击、无主动逻辑的纯触发植物——醒着被僵尸咬第一口时，蘑菇立即消失、
// 啃它的僵尸被魅惑（触发点在 Zombie::EatTarget 的植物分支，非本类）。
// 白天睡眠由 Shroom::SetupPlant 统一处理，睡着时按普通植物被啃、不触发魅惑。
class HypnoShroom : public Shroom {
public:
	using Shroom::Shroom;
};

#endif

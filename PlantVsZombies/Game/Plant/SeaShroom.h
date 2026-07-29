#pragma once

#include "PuffShroom.h"

/**
 * 只能直接种在水面的短程孢子射手。
 *
 * 攻击间隔、索敌、孢子弹与计时存档沿用 PuffShroom；仅动画事件、视觉和地形规则不同。
 */
class SeaShroom : public PuffShroom
{
public:
	using PuffShroom::PuffShroom;

protected:
	/** 配置水生表现，并在主人指定的全局第 33 帧发射孢子。 */
	void SetupPlant() override;
};

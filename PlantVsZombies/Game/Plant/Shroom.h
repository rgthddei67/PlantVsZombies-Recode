#pragma once
#ifndef _H_SHROOM_H
#define _H_SHROOM_H

#include "Plant.h"

class Shroom : public Plant
{
public:
	using Plant::Plant;
	void SetSleepState(bool sleep) override;

protected:
	void SetupPlant() override;
	/** 咖啡豆倒计时归零后的品种激活入口；默认回到 anim_idle。 */
	virtual void OnWakeUp();
};

#endif

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
	/** 返回睡眠与清醒待机包装轨；升级蘑菇可适配原版资源的不同轨名。 */
	virtual const char* GetSleepTrackName() const { return "anim_sleep"; }
	virtual const char* GetAwakeIdleTrackName() const { return "anim_idle"; }
	/** 咖啡豆倒计时归零后的品种激活入口；默认回到 anim_idle。 */
	virtual void OnWakeUp();
};

#endif

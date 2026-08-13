#pragma once

#include "SunFlower.h"

/**
 * 原版紫卡双子向日葵：只能覆盖向日葵，每轮同时生产两颗普通阳光。
 * 本项目强化版把基础生产间隔缩短为普通向日葵的 20 秒减 5 秒。
 */
class TwinSunflower : public SunFlower
{
public:
	using SunFlower::SunFlower;

	void SetupPlant() override;

protected:
	float GetProductionInterval() const override;
	int GetProductionSunCount() const override { return 2; }
};

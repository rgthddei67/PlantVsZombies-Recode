#include "TwinSunflower.h"

namespace {
	constexpr float kTwinProductionInterval = 15.0f; // 强化后的双子向日葵生产间隔，单位：游戏秒
	constexpr float kTwinInitialProduceTimer = 10.0f; // 保持约 5 秒首轮准备时间所需的初始进度
}

/** 初始化独立首轮进度，避免紫卡升级完成时立即兑现两颗阳光。 */
void TwinSunflower::SetupPlant()
{
	SunFlower::SetupPlant();
	mProduceTimer = kTwinInitialProduceTimer;
}

float TwinSunflower::GetProductionInterval() const
{
	return kTwinProductionInterval;
}

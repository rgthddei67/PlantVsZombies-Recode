#include "AdventureProgression.h"

#include <algorithm>

PlantType AdventureProgression::AdvanceProgress(int completedLevel, int& adventureLevel,
	std::vector<PlantType>& haveCards)
{
	if (!IsAdventureLevel(completedLevel) || adventureLevel != completedLevel)
		return NO_PLANT_REWARD;

	// 奖杯与选关跳过共用同一推进边沿；已有卡和无奖励关也正常解锁下一关。
	const PlantType reward = GetPlantReward(completedLevel);
	const bool unlock = reward != NO_PLANT_REWARD
		&& std::find(haveCards.begin(), haveCards.end(), reward) == haveCards.end();
	if (unlock) haveCards.push_back(reward);
	++adventureLevel;
	return unlock ? reward : NO_PLANT_REWARD;
}

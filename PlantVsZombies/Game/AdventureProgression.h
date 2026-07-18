#pragma once

#include "Plant/PlantType.h"

#include <array>
#include <cstddef>

namespace AdventureProgression
{
	inline constexpr int LEVELS_PER_AREA = 9;
	inline constexpr int ADVENTURE_AREA_COUNT = 5;
	inline constexpr int LAST_ADVENTURE_LEVEL = LEVELS_PER_AREA * ADVENTURE_AREA_COUNT;

	// NUM_PLANT_TYPES 在奖励表中表示“该关正常推进，但不解锁植物”。
	inline constexpr PlantType NO_PLANT_REWARD = PlantType::NUM_PLANT_TYPES;

	// 下标 0 对应内部关卡 1（显示为 1-1）。修改关卡奖励只需编辑此表；
	// 不要为调整解锁顺序而插入 PlantType 枚举，否则旧存档中的整数植物 ID 会错位。
	inline constexpr std::array<PlantType, LAST_ADVENTURE_LEVEL> PLANT_REWARD_BY_LEVEL = {
		// 1-1 ... 1-9（白天）
		PlantType::PLANT_SUNFLOWER,
		PlantType::PLANT_CHERRYBOMB,
		PlantType::PLANT_WALLNUT,
		PlantType::PLANT_POTATOMINE,
		PlantType::PLANT_SNOWPEA,
		PlantType::PLANT_CHOMPER,
		PlantType::PLANT_REPEATER,
		NO_PLANT_REWARD,
		PlantType::PLANT_PUFFSHROOM,

		// 2-1 ... 2-9（黑夜）
		PlantType::PLANT_SUNSHROOM,
		PlantType::PLANT_FUMESHROOM,
		PlantType::PLANT_HYPNOSHROOM,
		PlantType::PLANT_SCAREDYSHROOM,
		PlantType::PLANT_ICESHROOM,
		PlantType::PLANT_ICEFUMESHROOM,
		PlantType::PLANT_DOOMSHROOM,
		NO_PLANT_REWARD,
		PlantType::PLANT_LILYPAD,

		// 3-1 ... 3-9（泳池）
		PlantType::PLANT_SQUASH,
		PlantType::PLANT_THREEPEATER,
		PlantType::PLANT_TANGLEKELP,
		PlantType::PLANT_JALAPENO,
		PlantType::PLANT_SPIKEWEED,
		PlantType::PLANT_TORCHWOOD,
		PlantType::PLANT_TALLNUT,
		NO_PLANT_REWARD,
		PlantType::PLANT_SEASHROOM,

		// 4-1 ... 4-9（雾夜泳池）
		PlantType::PLANT_PLANTERN,
		PlantType::PLANT_CACTUS,
		PlantType::PLANT_BLOVER,
		PlantType::PLANT_SPLITPEA,
		PlantType::PLANT_STARFRUIT,
		PlantType::PLANT_PUMPKINSHELL,
		PlantType::PLANT_MAGNETSHROOM,
		NO_PLANT_REWARD,
		PlantType::PLANT_CABBAGEPULT,

		// 5-1 ... 5-9（屋顶；5-9 为最终 Boss 关）
		PlantType::PLANT_FLOWERPOT,
		PlantType::PLANT_KERNELPULT,
		PlantType::PLANT_INSTANT_COFFEE,
		PlantType::PLANT_GARLIC,
		PlantType::PLANT_UMBRELLA,
		PlantType::PLANT_MARIGOLD,
		PlantType::PLANT_MELONPULT,
		NO_PLANT_REWARD,
		NO_PLANT_REWARD,
	};

	/** 返回内部关卡号对应的大关编号；非正数关卡返回 0。 */
	constexpr int GetAreaNumber(int level)
	{
		return level > 0 ? (level - 1) / LEVELS_PER_AREA + 1 : 0;
	}

	/** 返回内部关卡号在大关内的编号；非正数关卡返回 0。 */
	constexpr int GetLevelNumberInArea(int level)
	{
		return level > 0 ? (level - 1) % LEVELS_PER_AREA + 1 : 0;
	}

	/** 判断关卡号是否属于当前五大关冒险流程。 */
	constexpr bool IsAdventureLevel(int level)
	{
		return level >= 1 && level <= LAST_ADVENTURE_LEVEL;
	}

	/** 查询通关植物奖励；NO_PLANT_REWARD 表示只推进关卡。 */
	constexpr PlantType GetPlantReward(int completedLevel)
	{
		return IsAdventureLevel(completedLevel)
			? PLANT_REWARD_BY_LEVEL[static_cast<std::size_t>(completedLevel - 1)]
			: NO_PLANT_REWARD;
	}

	static_assert(GetPlantReward(1) == PlantType::PLANT_SUNFLOWER);
	static_assert(GetPlantReward(8) == NO_PLANT_REWARD);
	static_assert(GetPlantReward(9) == PlantType::PLANT_PUFFSHROOM);
	static_assert(GetAreaNumber(18) == 2 && GetLevelNumberInArea(18) == 9);
	static_assert(GetAreaNumber(19) == 3 && GetLevelNumberInArea(19) == 1);
}

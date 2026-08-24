#pragma once

#include "Plant/PlantType.h"

#include <array>
#include <cstddef>

namespace AdventureProgression
{
	inline constexpr int LEVELS_PER_AREA = 9;
	inline constexpr int ADVENTURE_AREA_COUNT = 7;
	inline constexpr int LAST_ADVENTURE_LEVEL = LEVELS_PER_AREA * ADVENTURE_AREA_COUNT;
	inline constexpr int AREA_FIVE_BOSS_LEVEL = LEVELS_PER_AREA * 5;
	inline constexpr int AREA_SIX_FINAL_LEVEL = LEVELS_PER_AREA * 6;
	inline constexpr int AREA_SEVEN_FINAL_LEVEL = LEVELS_PER_AREA * 7;

	/** 冒险关的 BOSS 槽位；枚举值同时是关卡编排选择，不直接承担实体所有权。 */
	enum class BossSlot {
		NONE,
		ROOF_MARSHAL,
	};

	// NUM_PLANT_TYPES 在奖励表中表示“该关正常推进，但不解锁植物”。
	inline constexpr PlantType NO_PLANT_REWARD = PlantType::NUM_PLANT_TYPES;

	// TODO: 修改每关获得植物改这里

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
		PlantType::PLANT_ELITE_SCAREDYSHROOM,
		PlantType::PLANT_LILYPAD,

		// 3-1 ... 3-9（泳池）
		PlantType::PLANT_SQUASH,
		PlantType::PLANT_THREEPEATER,
		PlantType::PLANT_TANGLEKELP,
		PlantType::PLANT_JALAPENO,
		PlantType::PLANT_SPIKEWEED,
		PlantType::PLANT_TORCHWOOD,
		PlantType::PLANT_TALLNUT,
		PlantType::PLANT_TOXICPEASHOOTER,
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

		// 5-1 ... 5-9（白天屋顶；5-9 保留独立 BOSS 槽位）
		PlantType::PLANT_FLOWERPOT,
		PlantType::PLANT_KERNELPULT,
		PlantType::PLANT_INSTANT_COFFEE,
		PlantType::PLANT_GARLIC,
		PlantType::PLANT_UMBRELLA,
		PlantType::PLANT_MARIGOLD,
		PlantType::PLANT_MELONPULT,
		NO_PLANT_REWARD,
		PlantType::PLANT_GROUNDINGSHROOM,

		// 6-1 ... 6-9（黑夜屋顶；植物奖励与专属编排后续逐关补充）
		NO_PLANT_REWARD,
		PlantType::PLANT_GLOOMSHROOM,
		PlantType::PLANT_TWINSUNFLOWER,
		PlantType::PLANT_LIGHTNINGRODPOT,
		PlantType::PLANT_WINTERMELON,
		PlantType::PLANT_COBCANNON,
		PlantType::PLANT_GOLD_MAGNET,
		PlantType::PLANT_IMITATER,
		NO_PLANT_REWARD,

		// 7-1 ... 7-9（冬日花园；首株寒潮植物在 7-1 通关后解锁）
		PlantType::PLANT_SNOWANCHORNUT,
		NO_PLANT_REWARD,
		NO_PLANT_REWARD,
		NO_PLANT_REWARD,
		NO_PLANT_REWARD,
		NO_PLANT_REWARD,
		NO_PLANT_REWARD,
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

	/** 判断关卡号是否属于当前七大关冒险流程。 */
	constexpr bool IsAdventureLevel(int level)
	{
		return level >= 1 && level <= LAST_ADVENTURE_LEVEL;
	}

	/** 判断不依赖夜间泳池背景、但明确复用完整迷雾与路灯花机制的固定冒险关。 */
	constexpr bool HasLevelSpecificFogMechanics(int level)
	{
		switch (level) {
		case AREA_SIX_FINAL_LEVEL:
			return true;
		default:
			return false;
		}
	}

	/** 查询当前已登记的 BOSS 槽位。 */
	constexpr BossSlot GetBossSlot(int level)
	{
		return level == AREA_FIVE_BOSS_LEVEL
			? BossSlot::ROOF_MARSHAL
			: BossSlot::NONE;
	}

	/** 判断关卡是否已被正式标记为 BOSS 关。 */
	constexpr bool IsBossLevel(int level)
	{
		return GetBossSlot(level) != BossSlot::NONE;
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
	static_assert(GetPlantReward(17) == PlantType::PLANT_ELITE_SCAREDYSHROOM);
	static_assert(GetPlantReward(19) == PlantType::PLANT_SQUASH);
	static_assert(GetPlantReward(26) == PlantType::PLANT_TOXICPEASHOOTER);
	static_assert(GetPlantReward(27) == PlantType::PLANT_SEASHROOM);
	static_assert(GetPlantReward(35) == NO_PLANT_REWARD);
	static_assert(GetPlantReward(45) == PlantType::PLANT_GROUNDINGSHROOM);
	static_assert(GetPlantReward(46) == NO_PLANT_REWARD);
	static_assert(GetPlantReward(47) == PlantType::PLANT_GLOOMSHROOM);
	static_assert(GetPlantReward(48) == PlantType::PLANT_TWINSUNFLOWER);
	static_assert(GetPlantReward(49) == PlantType::PLANT_LIGHTNINGRODPOT);
	static_assert(GetPlantReward(50) == PlantType::PLANT_WINTERMELON);
	static_assert(GetPlantReward(51) == PlantType::PLANT_COBCANNON);
	static_assert(GetPlantReward(52) == PlantType::PLANT_GOLD_MAGNET);
	static_assert(GetPlantReward(53) == PlantType::PLANT_IMITATER);
	static_assert(GetAreaNumber(18) == 2 && GetLevelNumberInArea(18) == 9);
	static_assert(GetAreaNumber(19) == 3 && GetLevelNumberInArea(19) == 1);
	static_assert(IsBossLevel(AREA_FIVE_BOSS_LEVEL));
	static_assert(GetBossSlot(AREA_FIVE_BOSS_LEVEL) == BossSlot::ROOF_MARSHAL);
	static_assert(GetBossSlot(44) == BossSlot::NONE);
	static_assert(IsAdventureLevel(46) && IsAdventureLevel(54));
	static_assert(GetAreaNumber(46) == 6 && GetLevelNumberInArea(46) == 1);
	static_assert(GetAreaNumber(54) == 6 && GetLevelNumberInArea(54) == 9);
	static_assert(!HasLevelSpecificFogMechanics(53));
	static_assert(HasLevelSpecificFogMechanics(54));
	static_assert(GetPlantReward(54) == NO_PLANT_REWARD);
	static_assert(IsAdventureLevel(55) && IsAdventureLevel(63));
	static_assert(GetAreaNumber(55) == 7 && GetLevelNumberInArea(55) == 1);
	static_assert(GetPlantReward(55) == PlantType::PLANT_SNOWANCHORNUT);
	static_assert(GetAreaNumber(63) == 7 && GetLevelNumberInArea(63) == 9);
	static_assert(GetPlantReward(63) == NO_PLANT_REWARD);
}

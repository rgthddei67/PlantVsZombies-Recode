#pragma once

#include "PlantType.h"

/** 返回升级植物要求的基础植物；非升级植物返回 NUM_PLANT_TYPES。 */
constexpr PlantType GetUpgradeBasePlantType(PlantType type)
{
	switch (type) {
	case PlantType::PLANT_GLOOMSHROOM:
		return PlantType::PLANT_FUMESHROOM;
	default:
		return PlantType::NUM_PLANT_TYPES;
	}
}

/** 返回指定类型是否必须覆盖一株基础植物才能正式种植。 */
constexpr bool IsUpgradePlantType(PlantType type)
{
	return GetUpgradeBasePlantType(type) != PlantType::NUM_PLANT_TYPES;
}

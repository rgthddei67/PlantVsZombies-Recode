#pragma once

#include "PlantType.h"

#include <array>
#include <cstddef>

/** 植物相对逻辑锚点占用的一个普通层格子。 */
struct PlantFootprintCell {
	int rowOffset = 0;
	int columnOffset = 0;
};

/**
 * 植物的排他普通层占格；一个实体 ID 会原子写入全部有效格子。
 * 未来礼盒等允许覆盖植物的机制应使用独立承载关系，不能扩大此排他槽为任意堆叠容器。
 */
struct PlantFootprint {
	std::array<PlantFootprintCell, 4> cells{};
	std::size_t count = 1;
};

/** 返回植物类型相对其左侧逻辑锚点的排他普通层占格。 */
constexpr PlantFootprint GetPlantFootprint(PlantType type)
{
	PlantFootprint footprint{};
	footprint.cells[0] = { 0, 0 };
	if (type == PlantType::PLANT_COBCANNON) {
		footprint.cells[1] = { 0, 1 };
		footprint.count = 2;
	}
	return footprint;
}

/** 返回该类型是否跨越多个普通层格子。 */
constexpr bool IsMultiCellPlantType(PlantType type)
{
	return GetPlantFootprint(type).count > 1;
}

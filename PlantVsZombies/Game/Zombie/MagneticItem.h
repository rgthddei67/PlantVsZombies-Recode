#pragma once

#include "../Definit.h"

#include <string>

/**
 * @brief 僵尸交给磁力菇的离体金属物表现数据。
 *
 * 僵尸只负责给出当前装备贴图、稳定世界起点和 C# 口径的贴图左上角局部落点；
 * 磁力菇按缩放后贴图尺寸换算绘制中心，并持有飞行、充能与存档生命周期。
 */
struct MagneticItem {
	std::string textureKey;
	Vector worldPosition;
	Vector destinationOffset;
	float drawScale = 0.8f;
};

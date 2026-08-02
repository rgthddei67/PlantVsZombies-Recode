#pragma once

#include "../Definit.h"

#include <string>

/**
 * @brief 僵尸交给磁力菇的离体金属物表现数据。
 *
 * 僵尸只负责给出当前装备贴图、稳定世界起点和吸到磁力菇附近的局部落点；
 * 飞行、充能与存档生命周期由磁力菇持有。
 */
struct MagneticItem {
	std::string textureKey;
	Vector worldPosition;
	Vector destinationOffset;
	float drawScale = 0.8f;
};

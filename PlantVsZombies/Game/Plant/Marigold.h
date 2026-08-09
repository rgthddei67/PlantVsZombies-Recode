#pragma once

#include "Plant.h"

/**
 * 金盏花的基础观赏版本：只播放待机动画，不生产金币或其他资源。
 */
class Marigold : public Plant {
public:
	using Plant::Plant;
};

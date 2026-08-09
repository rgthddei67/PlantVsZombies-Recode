#pragma once

#include "Zombie.h"

/**
 * @brief 5-9 屋脊督军视觉样机；复用普通僵尸骨架和时间线，指挥能力后续独立接入。
 */
class RoofMarshalZombie : public Zombie {
public:
	using Zombie::Zombie;

protected:
	void SetupZombie() override;
	/** @brief 隐藏普通头部组并发射军帽与头一体的专属掉落粒子。 */
	void HeadDrop() override;
};

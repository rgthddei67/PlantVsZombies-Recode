#pragma once

#include "Bullet.h"

/** 仙人掌发射的帧伤尖刺；穿透状态与碰撞语义由 Bullet 按当前类型统一管理。 */
class SpikeBullet final : public Bullet
{
public:
	using Bullet::Bullet;
};

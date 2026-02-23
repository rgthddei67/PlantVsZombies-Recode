#pragma once
#ifndef _H_BULLETTYPE_H
#define _H_BULLETTYPE_H

constexpr int NULL_BULLET_ID = -1024;

enum class BulletType {
	BULLET_PEA,				// 豌豆
	BULLET_SNOWPEA,			// 冰豌豆
	BULLET_CABBAGE,			// 卷心菜
	BULLET_MELON,			// 西瓜
	BULLET_PUFF,			// 孢子
	BULLET_WINTERMELON,		// 冰瓜
	BULLET_FIREBALL,		// 火豌豆
	BULLET_STAR,			// 星星
	BULLET_SPIKE,			// 尖刺
	BULLET_BASKETBALL,		// 篮球
	BULLET_KERNEL,			// 玉米粒
	BULLET_COBBIG,			// 玉米加农炮
	BULLET_BUTTER,			// 黄油
	BULLET_ZOMBIE_PEA,		// 僵尸豌豆
	NUM_BULLETS,
};

#endif
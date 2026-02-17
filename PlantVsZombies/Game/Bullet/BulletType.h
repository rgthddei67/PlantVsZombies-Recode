#pragma once
#ifndef _H_BULLETTYPE_H
#define _H_BULLETTYPE_H

constexpr int NULL_BULLET_ID = -1024;

enum class BulletType {
	BULLET_PEA,				// Íã¶¹
	BULLET_SNOWPEA,			// ±ùÍã¶¹
	BULLET_CABBAGE,			// ¾íÐÄ²Ë
	BULLET_MELON,			// Î÷¹Ï
	BULLET_PUFF,			// æß×Ó
	BULLET_WINTERMELON,		// ±ù¹Ï
	BULLET_FIREBALL,		// »ðÍã¶¹
	BULLET_STAR,			// ÐÇÐÇ
	BULLET_SPIKE,			// ¼â´Ì
	BULLET_BASKETBALL,		// ÀºÇò
	BULLET_KERNEL,			// ÓñÃ×Á£
	BULLET_COBBIG,			// ÓñÃ×¼ÓÅ©ÅÚ
	BULLET_BUTTER,			// »ÆÓÍ
	BULLET_ZOMBIE_PEA,		// ½©Ê¬Íã¶¹
	NUM_BULLETS,
};

#endif
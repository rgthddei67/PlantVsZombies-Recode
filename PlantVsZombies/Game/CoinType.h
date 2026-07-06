#pragma once
#ifndef _COINTYPE_H
#define _COINTYPE_H

constexpr int NULL_COIN_ID = -1024;

enum class CoinType {
	COIN_NONE,
	COIN_SILVER,
	COIN_GOLD,
	COIN_DIAMOND,
	COIN_SUN,
	COIN_SMALLSUN,
	COIN_LARGESUN,
};

#endif
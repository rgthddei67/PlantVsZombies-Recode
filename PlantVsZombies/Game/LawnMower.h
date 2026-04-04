#pragma once
#ifndef _LAWN_MOWER_H
#define _LAWN_MOWER_H

#include "AnimatedObject.h"

constexpr int NULL_MOWER_ID = -1024;
class Board;

enum class MowerType {
	LAWN,
	WATER,
	ROOF
};

class Mower : public AnimatedObject {
private:
	int mRow;
	int mMowerID = NULL_MOWER_ID;
	MowerType mMowerType;

public:

	Mower(Board* board, MowerType type, AnimationType mowerType, float x, float y, int row, float scale = 1.0f);
	~Mower() = default;
};

#endif
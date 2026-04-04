#include "LawnMower.h"

Mower::Mower(Board* board, MowerType type, AnimationType mowerType, float x, float y, int row, float scale)
	: AnimatedObject(ObjectType::OBJECT_LAWNMOWER, board,
		Vector(x, y),
		mowerType,
		ColliderType::BOX,
		Vector(50, 60),
		Vector(0, 0),
		scale,
		"LawnMower",
		false)
{
	this->mRow = row;
	this->mMowerType = type;
}
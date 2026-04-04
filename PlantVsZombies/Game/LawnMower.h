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

enum class MowerState {
	IDLE,		// 静止等待
	MOVING		// 向右移动中
};

class Mower : public AnimatedObject {
public:
	int mRow;
	int mMowerID = NULL_MOWER_ID;
	MowerType mMowerType;
	MowerState mState = MowerState::IDLE;
	float mSpeed = 230.0f;

	Mower(Board* board, MowerType type, AnimationType animType,
		float x, float y, int row, float scale = 0.85f);

	void Update() override;
	void Die();
	void Trigger();

	Vector GetPosition() const;
	void SetPosition(const Vector& position);
	int GetSortingKey() const override { return this->mRow; }
};

#endif

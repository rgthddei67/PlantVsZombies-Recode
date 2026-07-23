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

/** 水路清洁车相对水面的视觉阶段；与 MowerState 正交，避免改变旧存档中的移动状态数值。 */
enum class MowerHeight {
	LAND,
	ENTERING,
	IN_POOL,
	EXITING
};

class Mower : public AnimatedObject {
public:
	int mRow = 0;
	int mMowerID = NULL_MOWER_ID;
	MowerType mMowerType = MowerType::LAWN;
	MowerState mState = MowerState::IDLE;
	MowerHeight mMowerHeight = MowerHeight::LAND; // 水位阶段，供存档原样恢复
	float mSpeed = 230.0f;
	float mPoolVisualOffsetY = 0.0f;              // 原始入水下沉深度，绘制时另叠加水中上移校正，单位：像素

	Mower(Board* board, MowerType type, AnimationType animType,
		float x, float y, int row, float scale = 0.85f);

	void Update() override;
	void Die();
	void Trigger();
	/** 只恢复水位视觉状态，不启动清洁车、播放声音或重置已恢复的动画轨道。 */
	void RestorePoolVisualState(MowerHeight height, float visualOffsetY);

	Vector GetPosition() const;
	void SetPosition(const Vector& position);
	Vector GetVisualPosition() const override;
	int GetSortingKey() const override { return this->mRow; }

private:
	/** 根据当前位置推进水路清洁车的入水、在水中与出水阶段。 */
	void UpdatePoolState(float deltaTime);
	/** 播放与当前水位阶段匹配的吞噬动画，并在结束后返回对应稳态轨道。 */
	void PlayPoolMowAnimation();
	/** 水中和过渡阶段不绘制陆地阴影。 */
	void UpdatePoolShadowVisibility();
};

#endif

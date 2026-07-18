#pragma once
#ifndef _TROPHY_H
#define _TROPHY_H

#include "GameObject.h"
#include "TransformComponent.h"
#include "ColliderComponent.h"
#include "ClickableComponent.h"

class Board;

// 关卡奖杯：一张可点击的静态图片（无动画、不参与碰撞、不被收集），
// 点击后触发胜利结算。不是金币也没有动画，故直接继承 GameObject。
class Trophy : public GameObject {
private:
	Board* mBoard = nullptr;
	TransformComponent* mTransform = nullptr;

	// 入场缩放动画（三次缓入 APPEAR_START_SCALE → BASE_SCALE）
	bool mAppearing = true;
	float mAppearTimer = 0.0f;

	bool mIsGrowing = false;
	float mGrowTimer = 0.0f;
	Vector mStartPosition;
	Vector mTargetPosition;
	bool mIsMoving = false;
	static constexpr float APPEAR_DURATION = 0.4f;
	static constexpr float APPEAR_START_SCALE = 0.2f;
	static constexpr float GROW_DURATION = 3.0f;
	static constexpr float BASE_SCALE = 0.6f;
	static constexpr float MAX_SCALE = 2.3f;
	static constexpr float INITIAL_GLOW_RADIUS = 60.0f;   // 初始光圈半径
	static constexpr float MAX_GLOW_RADIUS = 1500.0f;     // 最大光圈半径（覆盖整个屏幕）

public:
	Trophy(Board* board, const Vector& position);

	void Start()  override;
	void Update() override;
	void Draw(Graphics* g) override;

	Vector GetPosition() const;

private:
	void SetOnClickBack(ClickableComponent* click);
	/** 首次通关时推进冒险关卡，并按显式奖励表解锁植物。 */
	void AdvanceAdventureProgress();
	void SetPosition(const Vector& newPos);
	void SetScale(float scale);
	void UpdateAppearScale();
};

#endif

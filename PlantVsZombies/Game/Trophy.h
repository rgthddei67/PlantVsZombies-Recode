#pragma once
#ifndef _TROPHY_H
#define _TROPHY_H

#include "Coin.h"

class Trophy : public Coin {
private:
	bool mIsGrowing = false;
	float mGrowTimer = 0.0f;
	Vector mStartPosition;
	Vector mTargetPosition;
	bool mIsMoving = false;
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

protected:
	void SetOnClickBack(std::shared_ptr<ClickableComponent> click) override;
};

#endif
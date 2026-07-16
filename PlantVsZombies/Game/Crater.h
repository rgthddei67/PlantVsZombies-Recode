#pragma once
#ifndef _CRATER_H
#define _CRATER_H

#include "GameObject.h"
#include "TransformComponent.h"

class Board;

// 毁灭菇弹坑：占住一个格子阻止种植，计时结束自动消散。
// 轻量 GameObject（Trophy 先例）：不参与碰撞/点击，仅计时+绘制；
// 所有权在 GameObjectManager，Board 持 weak_ptr 簿记（寻址/存档）。
class Crater : public GameObject {
public:
	static constexpr float CRATER_DURATION = 180.0f;	// 原版 CRATER_TIME=18000cs=180s

	int mRow = 0;
	int mColumn = 0;
	float mTimeLeft = CRATER_DURATION;

	Crater(Board* board, int row, int column, float timeLeft = CRATER_DURATION);

	void Update() override;
	void Draw(Graphics* g) override;

private:
	static constexpr float FADE_OUT_TIME = 0.25f;	// 原版 counter<25cs 起 alpha 线性淡出

	Board* mBoard = nullptr;
	TransformComponent* mTransform = nullptr;
};

#endif

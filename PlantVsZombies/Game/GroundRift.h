#pragma once

#include "GameObject.h"

class Board;

/**
 * @brief 冰裂钻机提交后的独立同行地裂；按列向房屋传播并让雪锚果改变后续伤害倍率。
 * @details GameObjectManager 持强所有权，Board 只保存弱引用；来源僵尸提交后不再拥有它。
 */
class GroundRift final : public GameObject {
public:
	static constexpr int kPlantDamage = 177; // 每个新格首次被地裂扫过时的基础植物伤害
	static constexpr float kTravelSpeed = 180.0f; // 地裂前沿向房屋移动速度，单位 px/游戏秒

	GroundRift(Board* board, int row, float frontX,
		int nextColumn, float downstreamDamageMultiplier = 1.0f);

	void Update() override;
	void Draw(Graphics* g) override;
	int GetSortingKey() const override { return mRow; }

	int GetRow() const { return mRow; }
	float GetFrontX() const;
	int GetNextColumn() const { return mNextColumn; }
	float GetDownstreamDamageMultiplier() const {
		return mDownstreamDamageMultiplier;
	}

private:
	void ResolveCrossedColumns(float newFrontX);
	void ResolveColumn(int column);
	void Finish();

	Board* mBoard = nullptr;
	int mRow = 0;
	int mNextColumn = -1;
	float mDownstreamDamageMultiplier = 1.0f;
};

#pragma once

#include "GameObject.h"
#include "TransformComponent.h"

class Board;

/**
 * @brief 放置在植物格上的经典扶梯；Board 负责格子寻址，GameObjectManager 负责所有权。
 */
class Ladder final : public GameObject {
public:
	int mRow = 0;
	int mColumn = 0;

	Ladder(Board* board, int row, int column);

	void Draw(Graphics* g) override;
	int GetSortingKey() const override { return mRow; }
	/** 返回磁力菇离体物使用的当前贴图中心世界坐标。 */
	Vector GetVisualCenter() const;

private:
	Board* mBoard = nullptr;
	TransformComponent* mTransform = nullptr;
};

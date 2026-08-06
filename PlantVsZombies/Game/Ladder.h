#pragma once

#include "GameObject.h"
#include "TransformComponent.h"

class Board;

/** 已放置扶梯的外观样式；旧档和通用测试夹具默认使用 CLASSIC。 */
enum class LadderStyle {
	CLASSIC,
	ELITE,
};

/**
 * @brief 放置在植物格上的共享扶梯；样式随部署者锁定，Board 负责寻址，GameObjectManager 负责所有权。
 */
class Ladder final : public GameObject {
public:
	int mRow = 0;
	int mColumn = 0;

	Ladder(Board* board, int row, int column,
		LadderStyle style = LadderStyle::CLASSIC);

	void Draw(Graphics* g) override;
	int GetSortingKey() const override { return mRow; }
	/** 随植物组合切换逻辑格；绘制阶段继续消费宿主植物的阵风视觉偏移。 */
	void MoveToGridCell(int row, int column);
	/** 返回当前宿主植物的阵风瞬态偏移；没有宿主时为零。 */
	Vector GetGridMoveVisualOffset() const;
	/** 返回磁力菇离体物使用的当前贴图中心世界坐标。 */
	Vector GetVisualCenter() const;
	LadderStyle GetStyle() const { return mStyle; }
	const char* GetStyleName() const;
	/** 绘制与磁力菇离体物共用同一贴图键，避免精英扶梯被吸取时变回经典配色。 */
	const std::string& GetTextureKey() const;

private:
	Board* mBoard = nullptr;
	TransformComponent* mTransform = nullptr;
	LadderStyle mStyle = LadderStyle::CLASSIC;
};

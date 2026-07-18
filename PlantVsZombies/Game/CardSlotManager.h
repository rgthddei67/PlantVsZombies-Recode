#pragma once
#ifndef _CARD_SLOT_MANAGER_H
#define _CARD_SLOT_MANAGER_H

#include "Card.h"
#include "Component.h"
#include "./Plant/PlantType.h"
#include "Board.h"
#include "Cell.h"
#include <vector>
#include <memory>
#include <functional>

class GameObject;
class Plant;

class CardSlotManager : public Component {
private:
	std::vector<Card*> cards;  // 卡牌列表（观察者，所有权在 GameObjectManager）
	GameObject* selectedCard = nullptr;       // 当前选中的卡牌（观察者）
	Plant* plantPreview = nullptr;          // 植物预览（观察者，所有权在 GameObjectManager）
	Plant* cellPlantPreview = nullptr;

	// 常量参数
	Vector firstSlotPosition = Vector(64, -2); // 第一个卡牌槽的位置

	Board* mBoard = nullptr;
	Cell* mHoveredCell = nullptr;     // 当前鼠标悬停的Cell（观察者）

public:
	CardSlotManager(Board* board);

	void Start() override;
	void Update() override;
	bool NeedsUpdate() const override { return true; }
	void Draw(Graphics* g) override;
	void UpdateAllCardsState();
	void UpdatePreviewToMouse(const Vector& mousePos);

	// 卡牌操作
	void AddCard(Card* card);
	// 清空所有卡槽卡牌（销毁 GameObject）。用于生存模式轮间空槽重选。
	void ClearAllCards();
	void SelectCard(GameObject* card);
	void DeselectCard();

	bool CanAfford(int cost) const;   // 开发者作弊（无视阳光）守卫在 .cpp，避免头文件引 GameAPP.h
	bool SpendSun(int cost);

	// 清理植物预览
	void DestroyPlantPreview();

	// 处理Cell点击
	void HandleCellClick(int row, int col);

	// 移动预览到指定Cell
	void UpdatePreviewToCell(Cell* cell);

	// 获取当前选中的植物类型
	PlantType GetSelectedPlantType() const;

	// 获取卡牌信息
	GameObject* GetSelectedCard() const { return selectedCard; }
	int GetCurrentSun() const { return mBoard ? mBoard->GetSun() : 0; }
	Board* GetBoard() const { return mBoard; }
	const std::vector<Card*>& GetCards() const { return cards; }

private:
	void CreatePlantPreview(PlantType plantType);
	void UpdatePlantPreviewPosition(Graphics* g, const Vector& position);

	// 创建Cell悬停预览（透明）
	void CreateCellPlantPreview(PlantType plantType, Cell* cell);
	// 销毁Cell悬停预览
	void DestroyCellPlantPreview();

	// 检查是否可以在指定Cell放置植物
	bool CanPlaceInCell(Cell* cell) const;

	// 在指定Cell放置植物
	void PlacePlantInCell(int row, int col);
};

#endif

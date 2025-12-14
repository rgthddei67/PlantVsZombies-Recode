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

// 前向声明
class GameObject;
class Plant;

class CardSlotManager : public Component {
private:
    std::vector<std::shared_ptr<GameObject>> cards;           // 所有的卡牌
    std::shared_ptr<GameObject> selectedCard = nullptr;       // 当前选中的卡牌
    std::shared_ptr<Plant> plantPreview = nullptr;       // 植物预览

    // 布局参数
    Vector firstSlotPosition = Vector(64, -2); // 第一个卡牌的位置
    float slotSpacing = CARD_WIDTH + 5; // 卡牌间距 = 卡牌宽度 + 5像素空隙

    Board* mBoard = nullptr;
    std::weak_ptr<Cell> mHoveredCell;     // 当前鼠标悬停的Cell

public:
    CardSlotManager(Board* board);

    void Start() override;
    void Update() override;
    void UpdateAllCardsState();
	void UpdatePreviewToMouse(const Vector& mousePos);
    void Draw(SDL_Renderer* renderer) override;

    // 卡牌管理
    void AddCard(PlantType plantType, int sunCost, float cooldown);
    void SelectCard(std::shared_ptr<GameObject> card);
    void DeselectCard();
    void ArrangeCards();

    // 资源管理（消耗Board）
    bool CanAfford(int cost) const { return mBoard ? mBoard->GetSun() >= cost : false; }
    bool SpendSun(int cost);

    // 销毁植物预览
    void DestroyPlantPreview();

    // 处理Cell点击
    void HandleCellClick(int row, int col);

    // 更新预览位置到指定Cell
    void UpdatePreviewToCell(std::weak_ptr<Cell> cell);

    // 获取当前选中的植物类型
    PlantType GetSelectedPlantType() const;

    // 获取卡牌信息
    std::shared_ptr<GameObject> GetSelectedCard() const { return selectedCard; }
    int GetCurrentSun() const { return mBoard ? mBoard->GetSun() : 0; }
    const std::vector<std::shared_ptr<GameObject>>& GetCards() const { return cards; }

private:
    void CreatePlantPreview(PlantType plantType);
    void UpdatePlantPreviewPosition(const Vector& position);

    // 检查是否可以在指定Cell放置植物
    bool CanPlaceInCell(const std::shared_ptr<Cell>& cell) const;

    // 在指定Cell放置植物
    void PlacePlantInCell(int row, int col);
};

#endif
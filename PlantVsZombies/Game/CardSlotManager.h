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
    std::vector<std::weak_ptr<GameObject>> cards;           // 存储卡牌的弱引用
    std::weak_ptr<GameObject> selectedCard;                 // 当前选中的卡牌（弱引用）
    std::shared_ptr<Plant> plantPreview = nullptr;          // 植物预览
    std::shared_ptr<Plant> cellPlantPreview = nullptr;      

    // 常量参数
    Vector firstSlotPosition = Vector(64, -2); // 第一个卡牌槽的位置
    float slotSpacing = CARD_WIDTH + 5; // 卡牌间距 = 卡牌宽度 + 5像素间隔

    Board* mBoard = nullptr;
    std::weak_ptr<Cell> mHoveredCell;     // 当前鼠标悬停的Cell

public:
    CardSlotManager(Board* board);

    void Start() override;
    void Update() override;
    void UpdateAllCardsState();
    void UpdatePreviewToMouse(const Vector& mousePos);
    void Draw(SDL_Renderer* renderer) override;

    // 卡牌操作
    void AddCard(PlantType plantType, int sunCost, float cooldown);
    void SelectCard(std::weak_ptr<GameObject> card);
    void DeselectCard();
    void ArrangeCards();

    bool CanAfford(int cost) const { return mBoard ? mBoard->GetSun() >= cost : false; }
    bool SpendSun(int cost);

    // 清理植物预览
    void DestroyPlantPreview();

    // 处理Cell点击
    void HandleCellClick(int row, int col);

    // 移动预览到指定Cell
    void UpdatePreviewToCell(std::weak_ptr<Cell> cell);

    // 获取当前选中的植物类型
    PlantType GetSelectedPlantType() const;

    // 获取卡牌信息
    std::shared_ptr<GameObject> GetSelectedCard() const { return selectedCard.lock(); }
    int GetCurrentSun() const { return mBoard ? mBoard->GetSun() : 0; }
    const std::vector<std::weak_ptr<GameObject>>& GetCards() const { return cards; }

private:
    void CreatePlantPreview(PlantType plantType);
    void UpdatePlantPreviewPosition(const Vector& position);

    // 创建Cell悬停预览（透明）
    void CreateCellPlantPreview(PlantType plantType, std::shared_ptr<Cell> cell);
    // 销毁Cell悬停预览
    void DestroyCellPlantPreview();

    // 检查是否可以在指定Cell放置植物
    bool CanPlaceInCell(const std::shared_ptr<Cell>& cell) const;

    // 在指定Cell放置植物
    void PlacePlantInCell(int row, int col);
};

#endif
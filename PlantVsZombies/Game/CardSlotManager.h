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
    std::vector<std::shared_ptr<GameObject>> cards;           // 管理的卡牌
    std::shared_ptr<GameObject> selectedCard = nullptr;       // 当前选中的卡牌
    std::shared_ptr<GameObject> plantPreview = nullptr;       // 植物预览

    // 布局参数
    Vector firstSlotPosition = Vector(64, -2); // 第一个卡牌的位置
    float slotSpacing = CARD_WIDTH + 5; // 卡牌间距 = 卡牌宽度 + 5像素间隔

    Board* mBoard;

    // 网格参数（与Board保持一致）
    Vector gridStart = Vector(CELL_INITALIZE_POS_X, CELL_INITALIZE_POS_Y);
    Vector cellSize = Vector(CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y);
    int gridRows;
    int gridCols;

public:
    CardSlotManager(Board* board);

    void Start() override;
    void Update() override;
    void UpdateAllCardsState();
    void Draw(SDL_Renderer* renderer) override;

    // 卡牌管理
    void AddCard(PlantType plantType, int sunCost, float cooldown);
    void SelectCard(std::shared_ptr<GameObject> card);
    void DeselectCard();
    void ArrangeCards();

    // 阳光管理（委托给Board）
    bool CanAfford(int cost) const { return mBoard ? mBoard->GetSun() >= cost : false; }
    bool SpendSun(int cost);

    // 植物放置
    void ShowPlantPreview();
    void HidePlantPreview();
    bool CanPlacePlant(const Vector& worldPos);
    void PlacePlant(const Vector& worldPos);
    Vector GetGridPosition(const Vector& worldPos);

    // 获取选中的植物类型
    PlantType GetSelectedPlantType() const;

    // 获取管理器状态
    std::shared_ptr<GameObject> GetSelectedCard() const { return selectedCard; }
    int GetCurrentSun() const { return mBoard ? mBoard->GetSun() : 0; }
    const std::vector<std::shared_ptr<GameObject>>& GetCards() const { return cards; }

private:
    void CreatePlantPreview(PlantType plantType);
    void UpdatePlantPreviewPosition(const Vector& position);
    std::shared_ptr<class Plant> CreatePlantAtPosition(PlantType plantType, const Vector& gridPos);
    bool IsGridCellOccupied(const Vector& gridPos);
    std::shared_ptr<Cell> GetCellAtGridPosition(const Vector& gridPos);
};

#endif
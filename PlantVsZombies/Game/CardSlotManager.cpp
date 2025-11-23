#include "CardSlotManager.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "Card.h"
#include "CardComponent.h"
#include "../UI/InputHandler.h"
#include "../GameApp.h"
#include <iostream>

CardSlotManager::CardSlotManager(Board* board)
    : mBoard(board)
{
    if (!mBoard) {
        gridRows = 5;
        gridCols = 8;
        std::cerr << "Warning: CardSlotManager created without Board reference!" << std::endl;
    }
    else
    {
        gridRows = mBoard->mRows;
        gridCols = mBoard->mColumns;
    }
}

void CardSlotManager::Start() {
    std::cout << "CardSlotManager started with " << cards.size() << " cards" << std::endl;
    std::cout << "Initial sun: " << (mBoard ? mBoard->GetSun() : 0) << std::endl;
}

void CardSlotManager::Update() {
    static int lastSun = 0;
    // 处理选中的卡牌
    if (selectedCard) {
        auto& input = GameAPP::GetInstance().GetInputHandler();
        Vector mousePos = input.GetMousePosition();

        // 更新植物预览位置
        UpdatePlantPreviewPosition(mousePos);

        // 左键放置植物
        if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
            if (CanPlacePlant(mousePos)) {
                PlacePlant(mousePos);
            }
        }

        // 右键取消选择
        if (input.IsMouseButtonPressed(SDL_BUTTON_RIGHT)) {
            DeselectCard();
        }
    }

    // 检测阳光变化，更新所有卡牌状态
    if (mBoard && lastSun != mBoard->GetSun()) {
        lastSun = mBoard->GetSun();
        UpdateAllCardsState();
    }
}

void CardSlotManager::UpdateAllCardsState() {
    for (auto& card : cards) {
        if (auto cardComp = card->GetComponent<CardComponent>()) {
            // 如果卡牌正在冷却，不强制更新状态，只更新冷却进度
            if (cardComp->IsCooldown()) {
                // 只更新冷却进度显示
                if (auto display = card->GetComponent<CardDisplayComponent>()) {
                    float progress = 1.0f - (cardComp->GetCooldownProgress());
                    display->SetCooldownProgress(progress);
                }
            }
            else {
                // 不在冷却状态，才更新状态
                cardComp->ForceStateUpdate();
            }
        }
    }
}

void CardSlotManager::Draw(SDL_Renderer* renderer) {
    // 如果有选中的卡牌且可以放置，绘制网格高亮
    if (selectedCard && plantPreview) {
        auto& input = GameAPP::GetInstance().GetInputHandler();
        Vector mousePos = input.GetMousePosition();

        if (CanPlacePlant(mousePos)) {
            Vector gridPos = GetGridPosition(mousePos);
            Vector worldPos = Vector(
                gridStart.x + gridPos.x * cellSize.x,
                gridStart.y + gridPos.y * cellSize.y
            );

            // 绘制可放置的高亮
            SDL_Rect highlightRect = {
                static_cast<int>(worldPos.x),
                static_cast<int>(worldPos.y),
                static_cast<int>(cellSize.x),
                static_cast<int>(cellSize.y)
            };

            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // 半透明绿色
            SDL_RenderFillRect(renderer, &highlightRect);
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // 绿色边框
            SDL_RenderDrawRect(renderer, &highlightRect);
        }
        else {
            // 不可放置时显示红色高亮
            Vector gridPos = GetGridPosition(mousePos);
            Vector worldPos = Vector(
                gridStart.x + gridPos.x * cellSize.x,
                gridStart.y + gridPos.y * cellSize.y
            );

            SDL_Rect highlightRect = {
                static_cast<int>(worldPos.x),
                static_cast<int>(worldPos.y),
                static_cast<int>(cellSize.x),
                static_cast<int>(cellSize.y)
            };

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100); // 半透明红色
            SDL_RenderFillRect(renderer, &highlightRect);
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // 红色边框
            SDL_RenderDrawRect(renderer, &highlightRect);
        }
    }
}

void CardSlotManager::AddCard(PlantType plantType, int sunCost, float cooldown) {
    auto card = GameObjectManager::GetInstance().CreateGameObjectImmediate<Card>(
        plantType, sunCost, cooldown);

    cards.push_back(card);
    ArrangeCards();
#ifdef _DEBUG
    std::cout << "Added card: " << static_cast<int>(plantType)
        << " Cost: " << sunCost << " Cooldown: " << cooldown << std::endl;
#endif
}

void CardSlotManager::SelectCard(std::shared_ptr<GameObject> card) {
    if (!card) return;

    auto cardComp = card->GetComponent<CardComponent>();
    if (!cardComp) {
        std::cout << "Card has no CardComponent" << std::endl;
        return;
    }

    // 严格的就绪状态检查
    if (!cardComp->IsReady()) {
        std::cout << "Card is not ready for selection. State: "
            << static_cast<int>(cardComp->GetCardState()) << std::endl;
        return;
    }

    // 检查阳光是否足够
    if (!CanAfford(cardComp->GetSunCost())) {
        std::cout << "Not enough sun to select this card" << std::endl;
        return;
    }

    // 如果点击的是已选中的卡牌，取消选择
    if (selectedCard == card) {
        DeselectCard();
        return;
    }

    // 取消之前的选择
    if (selectedCard) {
        if (auto prevCardComp = selectedCard->GetComponent<CardComponent>()) {
            prevCardComp->SetSelected(false);
        }
    }

    // 选择新卡牌
    selectedCard = card;
    if (cardComp) {
        cardComp->SetSelected(true);
        CreatePlantPreview(cardComp->GetPlantType());
    }

    std::cout << "Selected card: " << static_cast<int>(GetSelectedPlantType()) << std::endl;
}

void CardSlotManager::DeselectCard() {
    if (selectedCard) {
        if (auto cardComp = selectedCard->GetComponent<CardComponent>()) {
            cardComp->SetSelected(false);
        }
        selectedCard = nullptr;
        HidePlantPreview();
    }

    std::cout << "Deselected card" << std::endl;
}

void CardSlotManager::ArrangeCards() {
    for (size_t i = 0; i < cards.size(); i++) {
        // 计算卡牌位置：起始位置 + 索引 * 间距
        Vector position = firstSlotPosition + Vector(slotSpacing * i, 0);

        if (auto transform = cards[i]->GetComponent<TransformComponent>()) {
            transform->SetPosition(position);
        }
    }

    std::cout << "Arranged " << cards.size() << " cards starting from ("
        << firstSlotPosition.x << ", " << firstSlotPosition.y << ")" << std::endl;
}

bool CardSlotManager::SpendSun(int cost) {
    if (!mBoard) {
        std::cout << "No Board reference, cannot spend sun" << std::endl;
        return false;
    }

    if (CanAfford(cost)) {
        mBoard->SubSun(cost);
        UpdateAllCardsState();
        std::cout << "Spent " << cost << " sun. Remaining: " << mBoard->GetSun() << std::endl;
        return true;
    }
    std::cout << "Not enough sun! Need: " << cost << " Have: " << mBoard->GetSun() << std::endl;
    return false;
}

void CardSlotManager::ShowPlantPreview() {
    if (plantPreview) {
        plantPreview->SetActive(true);
    }
}

void CardSlotManager::HidePlantPreview() {
    if (plantPreview) {
        plantPreview->SetActive(false);
    }
}

void CardSlotManager::CreatePlantPreview(PlantType plantType) {
    // 如果已有预览，先销毁
    if (plantPreview) {
        GameObjectManager::GetInstance().DestroyGameObject(plantPreview);
        plantPreview = nullptr;
    }

    // 创建新的预览对象
    plantPreview = GameObjectManager::GetInstance().CreateGameObject<GameObject>();
    plantPreview->SetName("PlantPreview");

    // 添加变换组件
    auto transform = plantPreview->AddComponent<TransformComponent>();

    // 添加显示组件（简化版，只显示一个半透明矩形）
    // 实际游戏中应该显示实际的植物精灵图

    std::cout << "Created plant preview for type: " << static_cast<int>(plantType) << std::endl;
}

void CardSlotManager::UpdatePlantPreviewPosition(const Vector& position) {
    if (plantPreview && selectedCard) {
        Vector gridPos = GetGridPosition(position);
        Vector worldPos = Vector(
            gridStart.x + gridPos.x * cellSize.x,
            gridStart.y + gridPos.y * cellSize.y
        );

        if (auto transform = plantPreview->GetComponent<TransformComponent>()) {
            transform->SetPosition(worldPos);
        }

        // 根据是否可以放置更新预览颜色
        // 实际游戏中应该改变精灵图的颜色或透明度
    }
}

bool CardSlotManager::CanPlacePlant(const Vector& worldPos) {
    if (!selectedCard) return false;

    Vector gridPos = GetGridPosition(worldPos);

    // 检查网格边界
    if (gridPos.x < 0 || gridPos.x >= gridCols ||
        gridPos.y < 0 || gridPos.y >= gridRows) {
        return false;
    }

    // 检查是否已有植物
    if (IsGridCellOccupied(gridPos)) {
        return false;
    }

    // 检查阳光是否足够
    if (auto cardComp = selectedCard->GetComponent<CardComponent>()) {
        if (!CanAfford(cardComp->GetSunCost())) {
            return false;
        }
    }

    return true;
}

void CardSlotManager::PlacePlant(const Vector& worldPos) {
    if (!selectedCard || !CanPlacePlant(worldPos)) {
        std::cout << "Cannot place plant here" << std::endl;
        return;
    }

    auto cardComp = selectedCard->GetComponent<CardComponent>();
    if (!cardComp) return;

    Vector gridPos = GetGridPosition(worldPos);
    Vector placePos = Vector(
        gridStart.x + gridPos.x * cellSize.x,
        gridStart.y + gridPos.y * cellSize.y
    );

    // 消耗阳光
    if (!SpendSun(cardComp->GetSunCost())) {
        return;
    }

    // 创建植物
    auto plant = CreatePlantAtPosition(cardComp->GetPlantType(), placePos);
    cardComp->StartCooldown();
    if (plant) {
        // 开始冷却


        std::cout << "Planted " << static_cast<int>(cardComp->GetPlantType())
            << " at grid (" << gridPos.x << ", " << gridPos.y << ")" << std::endl;
    }

    // 取消选择
    DeselectCard();
}

Vector CardSlotManager::GetGridPosition(const Vector& worldPos) {
    int gridX = static_cast<int>((worldPos.x - gridStart.x) / cellSize.x);
    int gridY = static_cast<int>((worldPos.y - gridStart.y) / cellSize.y);
    return Vector(static_cast<float>(gridX), static_cast<float>(gridY));
}

PlantType CardSlotManager::GetSelectedPlantType() const {
    if (selectedCard) {
        if (auto cardComp = selectedCard->GetComponent<CardComponent>()) {
            return cardComp->GetPlantType();
        }
    }
    return PlantType::PLANT_NONE;
}

bool CardSlotManager::IsGridCellOccupied(const Vector& gridPos) {
    // 获取对应位置的Cell
    auto cell = GetCellAtGridPosition(gridPos);
    if (!cell) return true; // 如果没有找到Cell，认为已被占用

    // TODO: 检查该Cell上是否已有植物
    // 需要实现Plant类后，通过Cell上的植物组件来判断
    return false; // 暂时返回false
}

std::shared_ptr<Cell> CardSlotManager::GetCellAtGridPosition(const Vector& gridPos) {
    if (!mBoard) return nullptr;

    int row = static_cast<int>(gridPos.y);
    int col = static_cast<int>(gridPos.x);

    return mBoard->GetCell(row, col);
}

std::shared_ptr<Plant> CardSlotManager::CreatePlantAtPosition(PlantType plantType, const Vector& position) {
    // 创建植物GameObject
    auto plantObj = GameObjectManager::GetInstance().CreateGameObject<GameObject>();
    plantObj->SetName("Plant");

    // 添加变换组件
    auto transform = plantObj->AddComponent<TransformComponent>();
    transform->SetPosition(position);

    // 添加植物组件（需要实现Plant类）
    // auto plant = plantObj->AddComponent<Plant>(plantType);

    std::cout << "Created plant at position (" << position.x << ", " << position.y << ")" << std::endl;

    // 返回植物组件
    return nullptr; // 暂时返回nullptr，实际应该返回plant组件
}
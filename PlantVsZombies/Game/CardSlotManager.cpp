#include "CardSlotManager.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "Card.h"
#include "CardComponent.h"
#include "../UI/InputHandler.h"
#include "../GameApp.h"
#include "AudioSystem.h"
#include "./Plant/Plant.h"
#include <iostream>

CardSlotManager::CardSlotManager(Board* board)
    : mBoard(board)
{
    if (!mBoard) {
        std::cerr << "Warning: CardSlotManager created without Board reference!" << std::endl;
    }
}

void CardSlotManager::Start() {
    std::cout << "CardSlotManager started with " << cards.size() << " cards" << std::endl;
    std::cout << "Initial sun: " << (mBoard ? mBoard->GetSun() : 0) << std::endl;

    // 为所有Cell设置点击回调
    if (mBoard) {
        for (int row = 0; row < mBoard->mRows; ++row) {
            for (int col = 0; col < mBoard->mColumns; ++col) {
                auto cell = mBoard->GetCell(row, col);
                if (cell) {
                    cell->SetClickCallback([this](int r, int c) {
                        HandleCellClick(r, c);
                        });
                }
            }
        }
    }
}

void CardSlotManager::Update() {
    static int lastSun = 0;

    // 如果有选中的卡牌，更新鼠标悬停的Cell
    if (selectedCard) {
        auto& input = GameAPP::GetInstance().GetInputHandler();
        Vector mousePos = input.GetMousePosition();

		UpdatePreviewToMouse(mousePos);
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
                if (auto display = cardComp->GetCardDisplayComponent()) {
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
    // 如果有选中的卡牌，绘制当前悬停Cell的高亮
    if (selectedCard) {
        auto& input = GameAPP::GetInstance().GetInputHandler();
        Vector mousePos = input.GetMousePosition();

        // 更新预览位置
        UpdatePlantPreviewPosition(mousePos);
        /*
        // 如果鼠标在某个Cell上，绘制高亮
        if (mHoveredCell && CanPlaceInCell(mHoveredCell)) {
            Vector worldPos = mHoveredCell->GetWorldPosition();

            // 绘制可放置的高亮
            SDL_Rect highlightRect = {
                static_cast<int>(worldPos.x),
                static_cast<int>(worldPos.y),
                static_cast<int>(CELL_COLLIDER_SIZE_X),
                static_cast<int>(CELL_COLLIDER_SIZE_Y)
            };

            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // 半透明绿色
            SDL_RenderFillRect(renderer, &highlightRect);
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // 绿色边框
            SDL_RenderDrawRect(renderer, &highlightRect);
        }
        */
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
        mHoveredCell.reset();
    }
	DestroyPlantPreview();
}

void CardSlotManager::ArrangeCards() {
    for (size_t i = 0; i < cards.size(); i++) {
        // 计算卡牌位置：起始位置 + 索引 * 间距
        Vector position = firstSlotPosition + Vector(slotSpacing * i, 0);

        if (auto transform = cards[i]->GetComponent<TransformComponent>()) {
            transform->SetPosition(position);
        }
    }
}

bool CardSlotManager::SpendSun(int cost) {
    if (!mBoard) {
        std::cout << "No Board reference, cannot spend sun" << std::endl;
        return false;
    }

    if (CanAfford(cost)) {
        mBoard->SubSun(cost);
        UpdateAllCardsState();
        return true;
    }
    return false;
}

void CardSlotManager::DestroyPlantPreview() {
    if (plantPreview) {
        plantPreview->Die();
        plantPreview = nullptr;
    }
}

void CardSlotManager::CreatePlantPreview(PlantType plantType) {
    DestroyPlantPreview();

    if (mBoard) {
        plantPreview = mBoard->CreatePlant(plantType, 0, 0, true);
        plantPreview->PauseAnimation();
    }
}

void CardSlotManager::UpdatePlantPreviewPosition(const Vector& position) {
    if (plantPreview && selectedCard) {
        // 查找鼠标所在的Cell
        std::shared_ptr<Cell> hoveredCell = nullptr;

        if (mBoard) {
            // 遍历查找鼠标所在的Cell
            for (int row = 0; row < mBoard->mRows; ++row) {
                for (int col = 0; col < mBoard->mColumns; ++col) {
                    auto cell = mBoard->GetCell(row, col);
                    if (cell) 
                    {
                        if (auto clickable = cell->GetComponent<ClickableComponent>()) {
                            if (clickable->IsMouseOver()) {
                                hoveredCell = cell;
                                break;
                            }
                        }
                    }
                }
                if (hoveredCell) break;
            }
        }

        mHoveredCell = hoveredCell;

        // 更新预览位置
        if (hoveredCell) {
            UpdatePreviewToCell(hoveredCell);
        }
    }
}

void CardSlotManager::UpdatePreviewToMouse(const Vector& mousePos) {
    if (plantPreview) {
        if (auto transform = plantPreview->GetComponent<TransformComponent>()) {
            transform->SetPosition(mousePos);
        }

        // 查找鼠标所在的Cell，更新预览植物的行列信息
        /*
        if (mBoard) {
            for (int row = 0; row < mBoard->mRows; ++row) {
                for (int col = 0; col < mBoard->mColumns; ++col) {
                    auto cell = mBoard->GetCell(row, col);
                    if (cell && cell->GetComponent<ClickableComponent>()) {
                        auto clickable = cell->GetComponent<ClickableComponent>();
                        if (clickable->IsMouseOver()) {
                            if (auto plantComp = plantPreview->GetComponent<Plant>()) {
                                plantComp->mRow = row;
                                plantComp->mColumn = col;
                            }
                            break;
                        }
                    }
                }
            }
        }
        */
    }
}

void CardSlotManager::UpdatePreviewToCell(std::weak_ptr<Cell> cell) {
    if (plantPreview) 
    {
        if (auto Cell = cell.lock())
        {
            Vector centerPos = Cell->GetCenterPosition();
            if (auto transform = plantPreview->GetComponent<TransformComponent>()) {
                transform->SetPosition(centerPos);
            }
        }
    }
}

void CardSlotManager::HandleCellClick(int row, int col) {
    if (!selectedCard) return;

    auto cell = mBoard ? mBoard->GetCell(row, col) : nullptr;
    if (!cell) return;

    if (CanPlaceInCell(cell)) {
        PlacePlantInCell(row, col);
    }
}

bool CardSlotManager::CanPlaceInCell(const std::shared_ptr<Cell>& cell) const {
    if (!selectedCard || !cell) return false;

    // 检查格子是否已有植物
    if (!cell->IsEmpty()) {
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

void CardSlotManager::PlacePlantInCell(int row, int col) {
    if (!selectedCard || !mBoard) return;

    auto cardComp = selectedCard->GetComponent<CardComponent>();
    if (!cardComp) return;

    auto cell = mBoard->GetCell(row, col);
    if (!cell) return;

    if (!SpendSun(cardComp->GetSunCost())) {
        return;
    }

    DestroyPlantPreview();
    AudioSystem::PlaySound(AudioConstants::SOUND_PLANT, 0.5f);

    // 创建植物
    auto plant = mBoard->CreatePlant(cardComp->GetPlantType(), row, col);

    if (plant) {
        cardComp->StartCooldown();

        std::cout << "Planted " << static_cast<int>(cardComp->GetPlantType())
            << " at cell (" << row << ", " << col << ")" << std::endl;
    }

    // 取消选择
    DeselectCard();
}

PlantType CardSlotManager::GetSelectedPlantType() const {
    if (selectedCard) {
        if (auto cardComp = selectedCard->GetComponent<CardComponent>()) {
            return cardComp->GetPlantType();
        }
    }
    return PlantType::PLANT_NONE;
}
#include "CardSlotManager.h"
#include "../ResourceKeys.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "Card.h"
#include "CardComponent.h"
#include "../UI/InputHandler.h"
#include "./Plant/GameDataManager.h"
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
	auto selected = selectedCard.lock();
	if (selected) {
		auto& input = GameAPP::GetInstance().GetInputHandler();
		Vector mouseScreen = input.GetMousePosition();  // 屏幕坐标

		UpdatePreviewToMouse(mouseScreen);              // 传入屏幕坐标
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
	for (auto& cardWeak : cards) {
		if (auto card = cardWeak.lock()) {
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
}

void CardSlotManager::Draw(SDL_Renderer* renderer) {
	// 如果有选中的卡牌，绘制当前悬停Cell的高亮
	if (auto selected = selectedCard.lock()) {
		auto& input = GameAPP::GetInstance().GetInputHandler();
		Vector mouseScreen = input.GetMousePosition();

		// 更新预览位置（
		UpdatePlantPreviewPosition(mouseScreen);
	}
}

void CardSlotManager::AddCard(std::shared_ptr<Card> card) {
	cards.push_back(card);
}

void CardSlotManager::SelectCard(std::weak_ptr<GameObject> cardWeak) {
	auto card = cardWeak.lock();
	if (!card) return;

	auto cardComp = card->GetComponent<CardComponent>();
	if (!cardComp) {
		std::cout << "Card has no CardComponent" << std::endl;
		return;
	}

	if (!cardComp->IsReady()) {
		return;
	}

	// 检查阳光是否足够
	if (!CanAfford(cardComp->GetSunCost())) {
		return;
	}

	// 如果点击的是已选中的卡牌，取消选择
	auto currentSelected = selectedCard.lock();
	if (currentSelected == card) {
		DeselectCard();
		return;
	}

	// 取消之前的选择
	if (currentSelected) {
		if (auto prevCardComp = currentSelected->GetComponent<CardComponent>()) {
			prevCardComp->SetSelected(false);
		}
	}

	// 选择新卡牌
	selectedCard = card;
	if (cardComp) {
		cardComp->SetSelected(true);
		CreatePlantPreview(cardComp->GetPlantType());
	}
}

void CardSlotManager::DeselectCard() {
	auto selected = selectedCard.lock();
	if (selected) {
		if (auto cardComp = selected->GetComponent<CardComponent>()) {
			cardComp->SetSelected(false);
		}
		selectedCard.reset();
		mHoveredCell.reset();
	}
	DestroyPlantPreview();
	DestroyCellPlantPreview();
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

void CardSlotManager::DestroyCellPlantPreview() {
	if (cellPlantPreview) {
		cellPlantPreview->Die();
		cellPlantPreview = nullptr;
	}
}

void CardSlotManager::CreatePlantPreview(PlantType plantType) {
	DestroyPlantPreview();

	if (mBoard) {
		plantPreview = mBoard->CreatePlant(plantType, 0, 0, true);
		plantPreview->PauseAnimation();
		plantPreview->SetRenderOrder(LAYER_EFFECTS + 10000);
	}
}

void CardSlotManager::CreateCellPlantPreview(PlantType plantType, std::shared_ptr<Cell> cell) {
	DestroyCellPlantPreview();

	if (mBoard && cell) {
		cellPlantPreview = mBoard->CreatePlant(plantType, 0, 0, true);
		if (cellPlantPreview) {
			GameDataManager& plantMgr = GameDataManager::GetInstance();
			Vector plantOffset = plantMgr.GetPlantOffset(plantType);

			Vector centerPos = cell->GetCenterPosition();          // 世界坐标
			Vector plantPosition = centerPos + plantOffset;        // 世界坐标

			cellPlantPreview->SetRenderOrder(LAYER_GAME_OBJECT - 5000);

			if (auto transform = cellPlantPreview->GetTransformComponent()) {
				transform->SetPosition(plantPosition);             // 设置为世界坐标
			}

			cellPlantPreview->SetAlpha(0.35f);
			cellPlantPreview->PauseAnimation();
		}
	}
}

void CardSlotManager::UpdatePlantPreviewPosition(const Vector& mouseScreen) {
	if (!plantPreview) return;

	auto selected = selectedCard.lock();
	if (!selected) return;

	// 将鼠标屏幕坐标转换为世界坐标
	Vector mouseWorld = GameAPP::GetInstance().GetCamera().ScreenToWorld(mouseScreen);

	std::shared_ptr<Cell> hoveredCell = nullptr;

	if (mBoard) {
		// 遍历所有 Cell，使用世界坐标检测鼠标是否在 Cell 的碰撞器内
		for (int row = 0; row < mBoard->mRows; ++row) {
			for (int col = 0; col < mBoard->mColumns; ++col) {
				auto cell = mBoard->GetCell(row, col);
				if (cell) {
					auto collider = cell->GetComponent<ColliderComponent>();
					if (collider && collider->mEnabled) {
						if (collider->ContainsPoint(mouseWorld)) {   // 使用世界坐标
							hoveredCell = cell;
							break;
						}
					}
				}
			}
			if (hoveredCell) break;
		}
	}

	bool isOverCellWithPlant = false;
	if (hoveredCell && hoveredCell->GetPlantID() != NULL_PLANT_ID) {
		isOverCellWithPlant = true;
	}

	if (isOverCellWithPlant) {
		DestroyCellPlantPreview();
		hoveredCell = nullptr;
	}

	auto oldHoveredCell = mHoveredCell.lock();
	if (hoveredCell != oldHoveredCell) {
		DestroyCellPlantPreview();

		if (hoveredCell) {
			if (auto cardComp = selected->GetComponent<CardComponent>()) {
				CreateCellPlantPreview(cardComp->GetPlantType(), hoveredCell);
			}
		}

		mHoveredCell = hoveredCell;
	}

	if (cellPlantPreview && hoveredCell) {
		if (auto cardComp = selected->GetComponent<CardComponent>()) {
			GameDataManager& plantMgr = GameDataManager::GetInstance();
			Vector plantOffset = plantMgr.GetPlantOffset(cardComp->GetPlantType());

			Vector centerPos = hoveredCell->GetCenterPosition();               // 世界坐标
			Vector plantPosition = centerPos + plantOffset;                    // 世界坐标

			if (auto transform = cellPlantPreview->GetComponent<TransformComponent>()) {
				transform->SetPosition(plantPosition);                         // 设置世界坐标
			}
		}
	}

	// 更新鼠标预览植物位置：直接使用世界坐标 + 偏移
	UpdatePreviewToMouse(mouseWorld);
}

void CardSlotManager::UpdatePreviewToMouse(const Vector& mouseWorld) {
	if (plantPreview) {
		GameDataManager& plantMgr = GameDataManager::GetInstance();
		Vector plantOffset = plantMgr.GetPlantOffset(plantPreview->mPlantType);

		// 预览植物位置 = 鼠标世界坐标 + 偏移
		Vector plantPosition = mouseWorld + plantOffset;

		if (auto transform = plantPreview->GetComponent<TransformComponent>()) {
			transform->SetPosition(plantPosition);      // 世界坐标
		}

		plantPreview->mRow = -1;
		plantPreview->mColumn = -1;
	}
}

void CardSlotManager::UpdatePreviewToCell(std::weak_ptr<Cell> cell) {
	if (plantPreview) {
		if (auto lockedCell = cell.lock()) {
			Vector centerPos = lockedCell->GetCenterPosition();      // 世界坐标
			if (auto transform = plantPreview->GetComponent<TransformComponent>()) {
				transform->SetPosition(centerPos);                  // 世界坐标
			}
		}
	}
}

void CardSlotManager::HandleCellClick(int row, int col) {
	auto selected = selectedCard.lock();
	if (!selected) return;

	auto cell = mBoard ? mBoard->GetCell(row, col) : nullptr;
	if (!cell) return;

	if (CanPlaceInCell(cell)) {
		PlacePlantInCell(row, col);
	}
}

bool CardSlotManager::CanPlaceInCell(const std::shared_ptr<Cell>& cell) const {
	auto selected = selectedCard.lock();
	if (!selected || !cell) return false;

	// 检查格子是否已有植物
	if (!cell->IsEmpty()) {
		return false;
	}

	// 检查阳光是否足够
	if (auto cardComp = selected->GetComponent<CardComponent>()) {
		if (!CanAfford(cardComp->GetSunCost())) {
			return false;
		}
	}

	return true;
}

void CardSlotManager::PlacePlantInCell(int row, int col) {
	auto selected = selectedCard.lock();
	if (!selected || !mBoard) return;

	auto cardComp = selected->GetComponent<CardComponent>();
	if (!cardComp) return;

	auto cell = mBoard->GetCell(row, col);
	if (!cell) return;

	if (!SpendSun(cardComp->GetSunCost())) {
		return;
	}

	DestroyPlantPreview();
	DestroyCellPlantPreview();
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PLANT, 0.5f);

	// 创建植物
	auto plant = mBoard->CreatePlant(cardComp->GetPlantType(), row, col);

	if (plant) {
		cardComp->StartCooldown();
	}

	// 取消选择
	DeselectCard();
}

PlantType CardSlotManager::GetSelectedPlantType() const {
	if (auto selected = selectedCard.lock()) {
		if (auto cardComp = selected->GetComponent<CardComponent>()) {
			return cardComp->GetPlantType();
		}
	}
	return PlantType::NUM_PLANT_TYPES;
}
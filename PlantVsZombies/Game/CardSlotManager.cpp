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
		Vector mousePos = input.GetMousePosition();

		// 更新预览位置
		UpdatePlantPreviewPosition(mousePos);
	}
}

void CardSlotManager::AddCard(PlantType plantType, int sunCost, float cooldown) {
	auto card = GameObjectManager::GetInstance().CreateGameObjectImmediate<Card>(
		LAYER_BACKGROUND, plantType, sunCost, cooldown);

	cards.push_back(card);
	ArrangeCards();
#ifdef _DEBUG
	std::cout << "Added card: " << static_cast<int>(plantType)
		<< " Cost: " << sunCost << " Cooldown: " << cooldown << std::endl;
#endif
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

void CardSlotManager::ArrangeCards() {
	for (size_t i = 0; i < cards.size(); i++) {
		if (auto card = cards[i].lock()) {
			// 计算卡牌位置：起始位置 + 索引 * 间距
			Vector position = firstSlotPosition + Vector(slotSpacing * i, 0);

			if (auto transform = card->GetComponent<TransformComponent>()) {
				transform->SetPosition(position);
			}
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
	}
}

void CardSlotManager::CreateCellPlantPreview(PlantType plantType, std::shared_ptr<Cell> cell) {
	DestroyCellPlantPreview();

	if (mBoard && cell) {
		cellPlantPreview = mBoard->CreatePlant(plantType, 0, 0, true);
		if (cellPlantPreview) {
			// 获取植物偏移量
			GameDataManager& plantMgr = GameDataManager::GetInstance();
			Vector plantOffset = plantMgr.GetPlantOffset(plantType);

			// 设置位置到Cell中心，并应用偏移量
			Vector centerPos = cell->GetCenterPosition();
			Vector plantPosition = centerPos + plantOffset;

			cellPlantPreview->SetRenderOrder(LAYER_GAME_OBJECT - 5000);

			if (auto transform = cellPlantPreview->GetTransformComponent()) {
				transform->SetPosition(plantPosition);
			}

			cellPlantPreview->SetAlpha(0.35f);
			cellPlantPreview->PauseAnimation();
		}
	}
}

void CardSlotManager::UpdatePlantPreviewPosition(const Vector& position) {
	if (plantPreview) {
		auto selected = selectedCard.lock();
		if (!selected) return;

		// 查找鼠标所在的Cell
		std::shared_ptr<Cell> hoveredCell = nullptr;

		if (mBoard) {
			// 遍历查找鼠标所在的Cell
			for (int row = 0; row < mBoard->mRows; ++row) {
				for (int col = 0; col < mBoard->mColumns; ++col) {
					auto cell = mBoard->GetCell(row, col);
					if (cell)
					{
						auto collider = cell->GetComponent<ColliderComponent>();
						if (collider && collider->mEnabled) {
							if (collider->ContainsPoint(position)) {
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
		if (hoveredCell && hoveredCell->GetPlantID() != NULL_PLANT_ID)
		{
			isOverCellWithPlant = true;
		}

		if (isOverCellWithPlant) {
			DestroyCellPlantPreview();
			hoveredCell = nullptr;
		}

		// 检查是否需要更新Cell预览
		auto oldHoveredCell = mHoveredCell.lock();
		if (hoveredCell != oldHoveredCell) {
			// Cell发生变化，销毁旧的Cell预览
			DestroyCellPlantPreview();

			// 如果悬停在新的Cell上，创建透明的Cell预览
			if (hoveredCell) {
				if (auto cardComp = selected->GetComponent<CardComponent>()) {
					CreateCellPlantPreview(cardComp->GetPlantType(), hoveredCell);
				}
			}

			mHoveredCell = hoveredCell;
		}

		// 更新Cell预览位置（如果存在）
		if (cellPlantPreview && hoveredCell) {
			// 获取植物偏移量
			if (auto cardComp = selected->GetComponent<CardComponent>()) {
				GameDataManager& plantMgr = GameDataManager::GetInstance();
				Vector plantOffset = plantMgr.GetPlantOffset(cardComp->GetPlantType());

				// 计算位置：Cell中心 + 偏移量
				Vector centerPos = hoveredCell->GetCenterPosition();
				Vector plantPosition = centerPos + plantOffset;

				if (auto transform = cellPlantPreview->GetComponent<TransformComponent>()) {
					transform->SetPosition(plantPosition);
				}
			}
		}

		// 更新鼠标预览位置
		UpdatePreviewToMouse(position);
	}
}

void CardSlotManager::UpdatePreviewToMouse(const Vector& mousePos) {
	if (plantPreview) {
		GameDataManager& plantMgr = GameDataManager::GetInstance();
		Vector plantOffset = plantMgr.GetPlantOffset(plantPreview->mPlantType);

		// 应用偏移量：鼠标位置 + 偏移
		Vector plantPosition = mousePos + plantOffset;

		if (auto transform = plantPreview->GetComponent<TransformComponent>()) {
			transform->SetPosition(plantPosition);
		}

		plantPreview->mRow = -1;
		plantPreview->mColumn = -1;
	}
}

void CardSlotManager::UpdatePreviewToCell(std::weak_ptr<Cell> cell) {
	if (plantPreview)
	{
		if (auto lockedCell = cell.lock())
		{
			Vector centerPos = lockedCell->GetCenterPosition();
			if (auto transform = plantPreview->GetComponent<TransformComponent>()) {
				transform->SetPosition(centerPos);
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
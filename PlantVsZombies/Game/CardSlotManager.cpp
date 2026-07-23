#include "CardSlotManager.h"
#include "../Logger.h"
#include "../ResourceKeys.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "Card.h"
#include "CardComponent.h"
#include "../UI/InputHandler.h"
#include "./Plant/GameDataManager.h"
#include "AudioSystem.h"
#include "./Plant/Plant.h"
#include "ShadowComponent.h"
#include "../GameAPP.h"

CardSlotManager::CardSlotManager(Board* board)
	: mBoard(board)
{
	if (!mBoard) {
		LOG_WARN("CardSlotManager") << "CardSlotManager created without Board reference!";
	}
}

void CardSlotManager::Start() {
	// 为所有Cell设置点击回调
	if (mBoard) {
		for (int row = 0; row < mBoard->mRows; ++row) {
			for (int col = 0; col < mBoard->mColumns; ++col) {
				Cell* cell = mBoard->GetCell(row, col);
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
	auto* selected = selectedCard;
	if (selected) {
		auto& input = GameAPP::GetInstance().GetInputHandler();
		Vector mouseScreen = input.GetMousePosition();  // 屏幕坐标

		UpdatePreviewToMouse(mouseScreen);              // 传入屏幕坐标
		// 右键取消选择
		if (input.IsMouseButtonPressed(SDL_BUTTON_RIGHT)) {
			DeselectCard();
			mBoard->mCursorObjectManager.ClearActive();
		}
	}

	// 检测阳光变化，更新所有卡牌状态
	if (mBoard && lastSun != mBoard->GetSun()) {
		lastSun = mBoard->GetSun();
		UpdateAllCardsState();
	}
}

void CardSlotManager::Draw(Graphics* g) {
	if (selectedCard) {
		Vector mouseScreen = GameAPP::GetInstance().GetInputHandler().GetMousePosition();

		// 更新预览位置
		UpdatePlantPreviewPosition(g, mouseScreen);
	}
}

void CardSlotManager::UpdateAllCardsState() {
	for (auto* card : cards) {
		if (!card) continue;
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

void CardSlotManager::AddCard(Card* card) {
	if (card) cards.push_back(card);
}

void CardSlotManager::ClearAllCards() {
	DeselectCard();
	for (auto* card : cards) {
		if (card) GameObjectManager::GetInstance().DestroyGameObject(card);
	}
	cards.clear();
	selectedCard = nullptr;
}

void CardSlotManager::SelectCard(GameObject* card) {
	if (!card) return;

	auto cardComp = card->GetComponent<CardComponent>();
	if (!cardComp) {
		LOG_ERROR("CardSlotManager") << "Card has no CardComponent";
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

	// 通过 CursorObjectManager 清除当前手持物（如铲子）
	mBoard->mCursorObjectManager.Activate(CursorObjectType::PLANT_PREVIEW, [this]() {
		DeselectCard();
		});

	// 选择新卡牌
	selectedCard = card;
	cardComp->SetSelected(true);
	CreatePlantPreview(cardComp->GetPlantType());
}

void CardSlotManager::DeselectCard() {
	if (selectedCard) {
		if (auto cardComp = selectedCard->GetComponent<CardComponent>()) {
			cardComp->SetSelected(false);
		}
		selectedCard = nullptr;
		mHoveredCell = nullptr;
	}
	DestroyPlantPreview();
	DestroyCellPlantPreview();
}

bool CardSlotManager::CanAfford(int cost) const {
	if (GameAPP::mDevelopMode && GameAPP::mDevFreePlant) return true;   // 开发者作弊：无视阳光
	return mBoard ? mBoard->GetSun() >= cost : false;
}

bool CardSlotManager::SpendSun(int cost) {
	if (!mBoard) {
		LOG_ERROR("CardSlotManager") << "No Board reference, cannot spend sun";
		return false;
	}

	if (GameAPP::mDevelopMode && GameAPP::mDevFreePlant) {
		UpdateAllCardsState();
		return true;                                   // 开发者作弊：视为支付成功但不扣阳光
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
		plantPreview = mBoard->CreatePlant(plantType, 0, 0, true, true);
		plantPreview->PauseAnimation();
		plantPreview->SetRenderOrder(LAYER_EFFECTS + 10000);
		plantPreview->RemoveComponent<ShadowComponent>();
	}
}

void CardSlotManager::CreateCellPlantPreview(PlantType plantType, Cell* cell) {
	DestroyCellPlantPreview();

	if (mBoard && cell) {
		cellPlantPreview = mBoard->CreatePlant(plantType, 0, 0, true, true);
		if (cellPlantPreview) {
			Vector centerPos = cell->GetCenterPosition();          // 世界坐标

			cellPlantPreview->SetRenderOrder(LAYER_BACKGROUND + 100);

			if (auto transform = cellPlantPreview->GetTransformComponent()) {
				transform->SetPosition(centerPos);             // 设置为世界坐标
			}

			cellPlantPreview->SetAlpha(0.35f);
			cellPlantPreview->RemoveComponent<ShadowComponent>();
			cellPlantPreview->PauseAnimation();
		}
	}
}

void CardSlotManager::UpdatePlantPreviewPosition(Graphics* g, const Vector& mouseScreen) {
	if (!plantPreview) return;

	auto* selected = selectedCard;
	if (!selected) return;

	// 屏幕坐标转世界坐标
	Vector mouseWorld = g->LogicalToWorld(mouseScreen.x, mouseScreen.y);

	Cell* hoveredCell = nullptr;

	if (mBoard) {
		// 遍历所有 Cell，使用世界坐标检测鼠标是否在 Cell 的碰撞器内
		for (int row = 0; row < mBoard->mRows; ++row) {
			for (int col = 0; col < mBoard->mColumns; ++col) {
				Cell* cell = mBoard->GetCell(row, col);
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
	if (hoveredCell && mBoard) {
		if (auto cardComp = selected->GetComponent<CardComponent>()) {
			isOverCellWithPlant = !mBoard->CanPlantAt(cardComp->GetPlantType(),
				hoveredCell->mRow, hoveredCell->mColumn);
		}
	}

	if (isOverCellWithPlant) {
		DestroyCellPlantPreview();
		hoveredCell = nullptr;
	}

	if (hoveredCell != mHoveredCell) {
		DestroyCellPlantPreview();

		if (hoveredCell) {
			if (auto cardComp = selected->GetComponent<CardComponent>()) {
				CreateCellPlantPreview(cardComp->GetPlantType(), hoveredCell);
			}
		}

		mHoveredCell = hoveredCell;
	}

	if (cellPlantPreview && hoveredCell) {
		if (selected->GetComponent<CardComponent>()) {
			Vector centerPos = hoveredCell->GetCenterPosition();               // 世界坐标

			if (auto transform = cellPlantPreview->GetTransformComponent()) {
				transform->SetPosition(centerPos);                         // 设置世界坐标
			}
		}
	}

	// 更新鼠标预览植物位置
	UpdatePreviewToMouse(mouseWorld);
}

void CardSlotManager::UpdatePreviewToMouse(const Vector& mouseWorld) {
	if (plantPreview) {
		if (auto transform = plantPreview->GetTransformComponent()) {
			transform->SetPosition(mouseWorld);      // 世界坐标
		}

		plantPreview->mRow = -1;
		plantPreview->mColumn = -1;
	}
}

void CardSlotManager::UpdatePreviewToCell(Cell* cell) {
	if (plantPreview && cell) {
		Vector centerPos = cell->GetCenterPosition();      // 世界坐标
		if (auto transform = plantPreview->GetTransformComponent()) {
			transform->SetPosition(centerPos);                  // 世界坐标
		}
	}
}

void CardSlotManager::HandleCellClick(int row, int col) {
	if (!selectedCard) return;

	Cell* cell = mBoard ? mBoard->GetCell(row, col) : nullptr;
	if (!cell) return;

	if (CanPlaceInCell(cell)) {
		PlacePlantInCell(row, col);
	}
}

bool CardSlotManager::CanPlaceInCell(Cell* cell) const {
	if (!selectedCard || !cell) return false;

	// 检查阳光是否足够
	if (auto cardComp = selectedCard->GetComponent<CardComponent>()) {
		if (!mBoard || !mBoard->CanPlantAt(cardComp->GetPlantType(),
			cell->mRow, cell->mColumn)) return false;
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

	Cell* cell = mBoard->GetCell(row, col);
	if (!cell) return;

	if (!SpendSun(cardComp->GetSunCost())) {
		return;
	}

	DestroyPlantPreview();
	DestroyCellPlantPreview();
	AudioSystem::PlaySound(mBoard->IsPoolSquare(row, col)
		? ResourceKeys::Sounds::SOUND_PLANT_ONWATER
		: ResourceKeys::Sounds::SOUND_PLANT, 0.5f);

	// 创建植物
	Plant* plant = mBoard->CreatePlant(cardComp->GetPlantType(), row, col);

	if (plant) {
		cardComp->StartCooldown();
	}

	// 取消选择
	DeselectCard();
	mBoard->mCursorObjectManager.ClearActive();
}

PlantType CardSlotManager::GetSelectedPlantType() const {
	if (selectedCard) {
		if (auto cardComp = selectedCard->GetComponent<CardComponent>()) {
			return cardComp->GetPlantType();
		}
	}
	return PlantType::NUM_PLANT_TYPES;
}

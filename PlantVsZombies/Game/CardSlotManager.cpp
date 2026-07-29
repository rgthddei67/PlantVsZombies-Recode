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
#include "./Plant/Plantern.h"
#include "ShadowComponent.h"
#include "../GameAPP.h"

namespace {
	constexpr float kPlanternMenuTopOffset = 74.0f; // 挡位菜单相对卡片顶部的纵向偏移，单位：UI px
	constexpr float kPlanternMenuButtonWidth = 50.0f; // 单个挡位按钮宽度，单位：UI px
	constexpr float kPlanternMenuButtonHeight = 21.0f; // 单个挡位按钮高度，单位：UI px
	constexpr float kPlanternMenuButtonGap = 2.0f; // 相邻挡位按钮的纵向间隔，单位：UI px

	/** 返回挡位菜单的逻辑锚点；始终直接对齐路灯花卡槽下方。 */
	Vector GetPlanternMenuAnchor(Card* card)
	{
		if (!card || !card->GetTransform()) return Vector::zero();
		const Vector cardAnchor = card->GetTransform()->GetPosition();
		return Vector(cardAnchor.x, cardAnchor.y + kPlanternMenuTopOffset);
	}
}

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
					cell->SetClickCallback([this](int, int) {
						// Cell 碰撞框共用边界且两侧都包含端点；点击也走预览的唯一解析，
						// 避免 ClickableComponent 的渲染顺序选择出另一个相邻格。
						const Vector mouseWorld =
							GameAPP::GetInstance().GetInputHandler().GetMouseWorldPosition();
						if (Cell* clickedCell = FindCellAtWorldPosition(mouseWorld)) {
							HandleCellClick(clickedCell->mRow, clickedCell->mColumn);
						}
						});
				}
			}
		}
	}
}

void CardSlotManager::Update() {
	static int lastSun = 0;
	UpdatePlanternGearMenuInput();

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
	DrawPlanternGearMenu(g);
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
	mPlanternGearMenuOpen = false;
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

	// 数量用尽时与阳光不足一样禁止选中，避免拿起一张全场都无法落下的卡。
	if (!CanUsePlant(cardComp->GetPlantType(), cardComp->GetSunCost())) {
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

void CardSlotManager::TogglePlanternGearMenu()
{
	if (!mBoard || !mBoard->GetActivePlantern() || !FindPlanternCard()) {
		mPlanternGearMenuOpen = false;
		return;
	}
	DeselectCard();
	mBoard->mCursorObjectManager.ClearActive();
	mPlanternGearMenuOpen = !mPlanternGearMenuOpen;
}

Card* CardSlotManager::FindPlanternCard() const
{
	for (Card* card : cards) {
		if (!card) continue;
		CardComponent* component = card->GetCardComponent();
		if (component && component->GetPlantType() == PlantType::PLANT_PLANTERN) {
			return card;
		}
	}
	return nullptr;
}

void CardSlotManager::UpdatePlanternGearMenuInput()
{
	if (!mPlanternGearMenuOpen) return;
	Card* card = FindPlanternCard();
	if (!mBoard || !mBoard->GetActivePlantern() || !card) {
		mPlanternGearMenuOpen = false;
		return;
	}

	auto& input = GameAPP::GetInstance().GetInputHandler();
	if (input.IsMouseButtonPressed(SDL_BUTTON_RIGHT)) {
		mPlanternGearMenuOpen = false;
		return;
	}
	if (!input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) return;

	const Vector anchor = GetPlanternMenuAnchor(card);
	const Vector mouse = input.GetMousePosition();
	for (int gear = 0; gear <= 3; ++gear) {
		const float y = anchor.y
			+ gear * (kPlanternMenuButtonHeight + kPlanternMenuButtonGap);
		if (mouse.x < anchor.x || mouse.x > anchor.x + kPlanternMenuButtonWidth
			|| mouse.y < y || mouse.y > y + kPlanternMenuButtonHeight) {
			continue;
		}
		mBoard->SetPlanternGear(static_cast<PlanternGear>(gear));
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CLICKSEED, 0.45f);
		return;
	}
}

void CardSlotManager::DrawPlanternGearMenu(Graphics* g)
{
	if (!g || !mPlanternGearMenuOpen || !mBoard || !mBoard->GetActivePlantern()) return;
	Card* card = FindPlanternCard();
	if (!card || !card->GetTransform()) return;

	const Vector logical = GetPlanternMenuAnchor(card);
	const Vector anchor = g->LogicalToWorld(logical.x, logical.y);
	const int currentGear = mBoard->GetPlanternGearValue();
	static const char* labels[] = { "0", "I", "II", "III" };
	const float panelHeight = 4.0f * kPlanternMenuButtonHeight
		+ 3.0f * kPlanternMenuButtonGap + 4.0f;
	g->FillRect(anchor.x - 2.0f, anchor.y - 2.0f,
		kPlanternMenuButtonWidth + 4.0f, panelHeight,
		glm::vec4(28.0f, 24.0f, 20.0f, 225.0f));

	for (int gear = 0; gear <= 3; ++gear) {
		const float y = anchor.y
			+ gear * (kPlanternMenuButtonHeight + kPlanternMenuButtonGap);
		const bool selected = gear == currentGear;
		g->FillRect(anchor.x, y, kPlanternMenuButtonWidth, kPlanternMenuButtonHeight,
			selected
				? glm::vec4(245.0f, 184.0f, 58.0f, 245.0f)
				: glm::vec4(86.0f, 76.0f, 62.0f, 235.0f));
		g->DrawGlyphRun(labels[gear], ResourceKeys::Fonts::FONT_FZCQ, 15,
			selected
				? glm::vec4(42.0f, 29.0f, 14.0f, 255.0f)
				: glm::vec4(245.0f, 235.0f, 205.0f, 255.0f),
			anchor.x + (gear < 2 ? 21.0f : 17.0f), y + 2.0f);
	}
}

bool CardSlotManager::CanUsePlant(PlantType type, int cost) const {
	return mBoard && CanAfford(cost) && mBoard->HasPlantingQuota(type);
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

			// 落点幽灵必须盖在该格已有承载植物之上，否则睡莲会遮住待种植物。
			Plant* topPlant = mBoard->GetTopPlantAt(cell->mRow, cell->mColumn);
			const int previewRenderOrder = topPlant
				? topPlant->GetRenderOrder() + 1 : LAYER_GAME_PLANT;
			cellPlantPreview->SetRenderOrder(previewRenderOrder);

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

	Cell* hoveredCell = FindCellAtWorldPosition(mouseWorld);

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

Cell* CardSlotManager::FindCellAtWorldPosition(const Vector& position) const {
	if (!mBoard) return nullptr;

	// ContainsPoint 对矩形四边均为闭区间；按固定行列顺序返回第一个命中，
	// 使水平、垂直乃至四格交点都只有一个稳定归属。
	for (int row = 0; row < mBoard->mRows; ++row) {
		for (int col = 0; col < mBoard->mColumns; ++col) {
			Cell* cell = mBoard->GetCell(row, col);
			if (!cell) continue;

			auto* collider = cell->GetComponent<ColliderComponent>();
			if (collider && collider->mEnabled && collider->ContainsPoint(position)) {
				return cell;
			}
		}
	}
	return nullptr;
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

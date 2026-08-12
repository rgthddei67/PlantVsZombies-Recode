#include "ChooseCardUI.h"
#include "SceneManager.h"
#include "../ResourceKeys.h"
#include "../ResourceManager.h"
#include "GameScene.h"
#include "./Plant/PlantType.h"
#include "./Plant/GameDataManager.h"
#include "./AudioSystem.h"
#include "../GameRandom.h"
#include "CardSlotManager.h"
#include "../GameApp.h"
#include <algorithm>
#include <memory>

namespace {
	constexpr float kRestoreButtonScale = 0.8f; // Button2 原图缩放倍率，避免遮挡面板右上角装饰
	constexpr float kRestoreButtonTextureWidth = 111.0f; // SeedChooser_Button2 原图宽度，单位：UI px
	constexpr float kRestoreButtonTextureHeight = 26.0f; // SeedChooser_Button2 原图高度，单位：UI px
	constexpr float kRestoreButtonRightInset = 12.0f; // 按钮右边缘距选卡面板右边缘，单位：UI px
	constexpr float kRestoreButtonTopInset = 8.0f; // 按钮上边缘距选卡面板上边缘，单位：UI px
}

ChooseCardUI::ChooseCardUI(GameScene* gameScene)
{
	this->mIsUI = true;
	this->SetName("ChooseCardUI");
	mCards.reserve(64);
	mSelectedCards.reserve(16);
	mGameScene = gameScene;
	if (!mGameScene) return;
	mTransform = AddComponent<TransformComponent>(60.0f, 800.0f);

	mCardUITexture = ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BACKGROUND);
	auto button = mGameScene->GetUIManager().CreateButton
	(Vector(360, 550),
		Vector(156 * 0.9f, 42 * 0.9f));
	mButton = button;
	button->SetAsCheckbox(false);
	button->SetImageKeys(ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON_DISABLED,
		ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON,
		ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON);
	button->SetTextColor({ 211, 157, 42 ,255 });
	button->SetHoverTextColor({ 211, 157, 42 ,255 });
	button->SetText(u8"  一起摇滚吧！", ResourceKeys::Fonts::FONT_FZCQ, 20);
	button->SetEnabled(false);
	button->SetClickCallBack([this](bool isChecked) {
		if (mGameScene) {
			mGameScene->ChooseCardComplete();
		}
		});

	auto restoreButton = mGameScene->GetUIManager().CreateButton(
		Vector::zero(),
		Vector(kRestoreButtonTextureWidth * kRestoreButtonScale,
			kRestoreButtonTextureHeight * kRestoreButtonScale));
	mRestoreButton = restoreButton;
	restoreButton->SetAsCheckbox(false);
	restoreButton->SetImageKeys(ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON2,
		ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON2_GLOW,
		ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON2);
	restoreButton->SetText(u8"上次选卡", ResourceKeys::Fonts::FONT_FZCQ, 13);
	restoreButton->SetTextColor({ 42, 42, 90, 255 });
	restoreButton->SetHoverTextColor({ 42, 42, 90, 255 });
	restoreButton->SetEnabled(false);
	restoreButton->SetClickCallBack([this](bool) {
		RestoreLastSelectedCards();
		});
	SyncRestoreButtonPosition();
}

ChooseCardUI::~ChooseCardUI() {
	SceneManager::GetInstance().GetCurrectSceneUIManager().RemoveButton(mButton.lock());
	SceneManager::GetInstance().GetCurrectSceneUIManager().RemoveButton(mRestoreButton.lock());
	mGameScene = nullptr;
	// TODO: 物体析构的时候，如果有其他物体没有销毁，不要在这个时候销毁，因为没用
}

void ChooseCardUI::Update() {
	GameObject::Update();
	SyncRestoreButtonPosition();
}

void ChooseCardUI::SetPosition(const Vector& position) {
	if (mTransform) mTransform->SetPosition(position);
	SyncRestoreButtonPosition();
}

void ChooseCardUI::RemoveAllCards() {
	for (auto* card : mCards) {
		GameObjectManager::GetInstance().DestroyGameObject(card);
	}
	mCards.clear();
	mSelectedCards.clear();
}

void ChooseCardUI::TransferSelectedCardsTo(CardSlotManager* manager) {
	for (auto* card : mSelectedCards) {
		// 设置卡牌状态为游戏内
		card->SetIsInChooseCardUI(false);
		if (auto comp = card->GetCardComponent()) {
			comp->SetIsInChooseCardUI(false);
			comp->SetCardGameClick(card);
		}
		// 添加到卡槽管理器
		if (manager) {
			manager->AddCard(card);
		}
		// 从 mCards 中移除
		auto it = std::find(mCards.begin(), mCards.end(), card);
		if (it != mCards.end()) {
			mCards.erase(it);
		}
	}
	mSelectedCards.clear();
}

void ChooseCardUI::Draw(Graphics* g) {
	// 绘制背景
	if (mCardUITexture) {
		Vector pos = this->GetPosition();
		Vector newpos = g->LogicalToWorld(pos.x, pos.y);
		int w = mCardUITexture->width;
		int h = mCardUITexture->height;

		g->DrawTexture(mCardUITexture,
			newpos.x, newpos.y,
			static_cast<float>(w), static_cast<float>(h));
	}
}

void ChooseCardUI::AddCard(PlantType type) {
	// 计算当前卡牌数量对应的行列
	int cardCount = static_cast<int>(mCards.size());
	int row = cardCount / MAX_CARDS_PER_ROW;
	int col = cardCount % MAX_CARDS_PER_ROW;

	// 计算位置
	float posX = START_X + col * (CARD_WIDTH + CARD_HORIZONTAL_SPACING);
	float posY = START_Y + row * (CARD_HEIGHT + CARD_VERTICAL_SPACING);

	auto& gameMgr = GameDataManager::GetInstance();

	auto card = GameObjectManager::GetInstance().
		CreateGameObjectImmediate<Card>(LAYER_UI, type,
			gameMgr.GetPlantSunCost(type), gameMgr.GetPlantCooldown(type), true);

	if (auto transform = card->GetComponent<TransformComponent>()) {
		transform->SetPosition(Vector(posX, posY));
	}
	card->SetOriginalPosition(Vector(posX, posY));
	card->mIsUI = true;
	mCards.push_back(card);
}

void ChooseCardUI::RemoveCard(Card* card)
{
	auto it = std::find(mCards.begin(), mCards.end(), card);
	if (it != mCards.end()) {
		mCards.erase(it);
	}

	// 如果已选中，也从选中列表中移除
	auto selIt = std::find(mSelectedCards.begin(), mSelectedCards.end(), card);
	if (selIt != mSelectedCards.end()) {
		mSelectedCards.erase(selIt);
		UpdateTargetPositions();
	}

	GameObjectManager::GetInstance().DestroyGameObject(card);
}

void ChooseCardUI::AddAllCard() {
	const auto& haveCards = GameAPP::GetInstance().mHaveCards;
	auto& gameData = GameDataManager::GetInstance();
	for (const auto& card : haveCards) {
		// 冒险进度可以先记录后续关卡奖励；对应植物尚未实装时先不把空工厂卡放进选卡界面。
		if (!gameData.HasPlant(card)) continue;
		AddCard(card);
	}
	RefreshRestoreButtonState();
}

Card* ChooseCardUI::FindCardByType(PlantType type) {
	for (auto* card : mCards) {
		if (!card) continue;
		auto* comp = card->GetCardComponent();
		if (comp && comp->GetPlantType() == type) return card;
	}
	return nullptr;
}

bool ChooseCardUI::ToggleCardSelection(Card* card) {
	if (!card) return false;

	auto it = std::find(mSelectedCards.begin(), mSelectedCards.end(), card);
	if (it != mSelectedCards.end()) {
		// 已选中 -> 移除
		mSelectedCards.erase(it);
		UpdateTargetPositions();
		return false;
	}
	else {
		// 未选中 -> 添加，检查数量限制
		if (mSelectedCards.size() >= MAX_SELECTED) {
			return false;
		}
		mSelectedCards.push_back(card);
		UpdateTargetPositions();
		return true;
	}
}

std::vector<PlantType> ChooseCardUI::GetSelectedCardTypes() {
	std::vector<PlantType> types;
	types.reserve(mSelectedCards.size());
	for (Card* card : mSelectedCards) {
		if (!card) continue;
		auto* component = card->GetCardComponent();
		if (component) types.push_back(component->GetPlantType());
	}
	return types;
}

std::vector<Card*> ChooseCardUI::ResolveRestorableCards() {
	std::vector<Card*> cards;
	cards.reserve(MAX_SELECTED);
	std::vector<PlantType> seenTypes;
	seenTypes.reserve(MAX_SELECTED);
	auto& gameData = GameDataManager::GetInstance();
	for (const std::string& cardName : GameAPP::GetInstance().mLastSelectedCards) {
		const PlantType type = gameData.StringToPlantType(cardName);
		if (type == PlantType::NUM_PLANT_TYPES
			|| gameData.PlantTypeToEnumName(type) != cardName
			|| std::find(seenTypes.begin(), seenTypes.end(), type) != seenTypes.end()) {
			continue;
		}
		Card* card = FindCardByType(type);
		if (!card) continue;
		seenTypes.push_back(type);
		cards.push_back(card);
		if (cards.size() >= MAX_SELECTED) break;
	}
	return cards;
}

bool ChooseCardUI::RestoreLastSelectedCards() {
	auto restoredCards = ResolveRestorableCards();
	if (restoredCards.empty()) return false;

	// 整组替换当前选择后只刷新一次目标位置，使全部卡片沿既有飞行动画进入对应槽位。
	mSelectedCards = std::move(restoredCards);
	UpdateTargetPositions();
	return true;
}

void ChooseCardUI::SyncRestoreButtonPosition() {
	auto button = mRestoreButton.lock();
	if (!button || !mTransform || !mCardUITexture) return;
	const Vector panelPosition = mTransform->GetPosition();
	const float buttonWidth = kRestoreButtonTextureWidth * kRestoreButtonScale;
	button->SetPosition(Vector(
		panelPosition.x + static_cast<float>(mCardUITexture->width)
			- buttonWidth - kRestoreButtonRightInset,
		panelPosition.y + kRestoreButtonTopInset));
}

void ChooseCardUI::RefreshRestoreButtonState() {
	if (auto button = mRestoreButton.lock()) {
		button->SetEnabled(!ResolveRestorableCards().empty());
	}
}

void ChooseCardUI::UpdateTargetPositions() {
	// 为所有卡牌计算目标位置
	int random = GameRandom::Range(0, 1);
	if (random == 0)
	{
		AudioSystem::PlaySound
		(ResourceKeys::Sounds::SOUND_CHOOSEPLANT1, 0.4f);
	}
	else
	{
		AudioSystem::PlaySound
		(ResourceKeys::Sounds::SOUND_CHOOSEPLANT2, 0.4f);
	}

	for (size_t i = 0; i < mCards.size(); i++) {
		auto* card = mCards[i];
		Vector targetPos = Vector(0, 0);
		// 检查是否在选中列表中
		auto it = std::find(mSelectedCards.begin(), mSelectedCards.end(), card);
		if (it != mSelectedCards.end()) {
			// 根据在列表中的索引计算槽位位置
			int index = static_cast<int>(it - mSelectedCards.begin());
			targetPos.x = SLOT_START_X + index * SLOT_SPACING;
			targetPos.y = SLOT_START_Y;
		}
		else {
			// 返回原始位置
			targetPos = card->GetOriginalPosition();
		}
		// 设置目标位置，启动动画
		card->SetTargetPosition(targetPos);
	}
}

bool ChooseCardUI::IsCardSelected(Card* card) const {
	return std::find(mSelectedCards.begin(), mSelectedCards.end(), card) != mSelectedCards.end();
}

float ChooseCardUI::GetGameSlotRightEdge() {
	return SLOT_START_X + static_cast<float>((MAX_SELECTED - 1) * SLOT_SPACING + CARD_WIDTH);
}

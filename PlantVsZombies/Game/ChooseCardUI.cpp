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
#include <memory>

ChooseCardUI::ChooseCardUI(GameScene* gameScene)
{
	this->SetName("ChooseCardUI");
	mCards.reserve(64);
	mSelectedCards.reserve(16);
	mGameScene = gameScene;
	mTransform = AddComponent<TransformComponent>(60.0f, 800.0f);
	mCardUITexture = ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BACKGROUND);
	mButton = SceneManager::GetInstance().GetCurrectSceneUIManager().CreateButton
	(Vector(400, 550),
		Vector(156 * 0.9f, 42 * 0.9f));
	mButton->SetAsCheckbox(false);
	mButton->SetImageKeys(ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON_DISABLED,
		ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON,
		ResourceKeys::Textures::IMAGE_SEEDCHOOSER_BUTTON);
	mButton->SetTextColor(SDL_Color{ 211, 157, 42 ,255 });
	mButton->SetHoverTextColor(SDL_Color{ 211, 157, 42 ,255 });
	mButton->SetText(u8"     一起摇滚吧！", ResourceKeys::Fonts::FONT_FZCQ, 20);
	mButton->SetEnabled(false);
	mButton->SetClickCallBack([this](bool isChecked) {
		if (mGameScene) {
			mGameScene->ChooseCardComplete();
		}
		});
}

ChooseCardUI::~ChooseCardUI() {
	SceneManager::GetInstance().GetCurrectSceneUIManager().RemoveButton(mButton);
	mGameScene = nullptr;
	//TODO 物体析构的时候，如果有其他物体没有销毁，不要在这个时候销毁，因为没用
}

void ChooseCardUI::RemoveAllCards() {
	for (auto& card : mCards) {
		GameObjectManager::GetInstance().DestroyGameObject(card);
	}
	mCards.clear();
	mSelectedCards.clear();
}

void ChooseCardUI::TransferSelectedCardsTo(CardSlotManager* manager) {
	for (auto& card : mSelectedCards) {
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

void ChooseCardUI::Draw(SDL_Renderer* renderer) {
	// 绘制背景
	if (mCardUITexture) {
		Vector pos = this->GetPosition();
		int w, h;
		SDL_QueryTexture(mCardUITexture, nullptr, nullptr, &w, &h);
		SDL_FRect dest =
		{
		pos.x,
		pos.y,
		static_cast<float>(w),
		static_cast<float>(h) };
		SDL_RenderCopyF(renderer, mCardUITexture, nullptr, &dest);
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

	mCards.push_back(card);
}

void ChooseCardUI::RemoveCard(std::shared_ptr<Card> card)
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
	for (int i = 0; i < static_cast<int>(PlantType::PLANT_CHERRYBOMB); ++i) {
		AddCard(static_cast<PlantType>(i));
	}
}

bool ChooseCardUI::ToggleCardSelection(std::shared_ptr<Card> card) {
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

	for (size_t i = 0; i < mCards.size(); ++i) {
		auto card = mCards[i];
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

bool ChooseCardUI::IsCardSelected(std::shared_ptr<Card> card) const {
	return std::find(mSelectedCards.begin(), mSelectedCards.end(), card) != mSelectedCards.end();
}
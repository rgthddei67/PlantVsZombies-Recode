#include "CardComponent.h"
#include "../ResourceKeys.h"
#include "CardDisplayComponent.h"
#include "ClickableComponent.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./CardSlotManager.h"
#include "../DeltaTime.h"
#include "./ChooseCardUI.h"
#include "AudioSystem.h"

CardComponent::CardComponent(PlantType type, int cost, float cooldown)
	: mPlantType(type), mSunCost(cost), mCooldownTime(cooldown)
{
	mCooldownTimer = 0.0f;
}

void CardComponent::Start() {
	if (auto manager = FindCardSlotManager()) {
		mCardSlotManager = manager;
	}

	auto gameObject = GetGameObject();

	auto card = std::dynamic_pointer_cast<Card>(gameObject);
	if (!card) return;

	mIsInChooseCardUI = card->GetIsInChooseCardUI();
	if (mIsInChooseCardUI) {
		SetCardChooseClick(gameObject, card);
	}
	else
	{
		SetCardGameClick(gameObject);
	}
}

void CardComponent::SetCardChooseClick(std::shared_ptr<GameObject> gameObject, 
	std::shared_ptr<Card> card)
{
	if (auto clickable = gameObject->GetComponent<ClickableComponent>()) {
		// ---------- 选卡界面点击逻辑 ----------
		clickable->onClick = [this, card]() {
			if (card->IsMoving()) return;

			// 查找 ChooseCardUI
			auto& manager = GameObjectManager::GetInstance();
			std::shared_ptr<ChooseCardUI> chooseUI;
			for (auto obj : manager.GetAllGameObjects()) {
				if (obj && obj->GetName() == "ChooseCardUI") {
					chooseUI = std::dynamic_pointer_cast<ChooseCardUI>(obj);
					break;
				}
			}

			if (!chooseUI) {
				std::cerr << "ChooseCardUI not found!" << std::endl;
				return;
			}

			// 切换选中状态
			bool isPickedUp = chooseUI->ToggleCardSelection(card);

			};
	}
}

void CardComponent::SetCardGameClick(std::shared_ptr<GameObject> gameObject)
{
	// 这里CardDisplayComponent还没有创建好，所以不能缓存 放到GetCardDisplayComponent里加载
	// 获取点击组件并设置回调
	if (auto clickable = gameObject->GetComponent<ClickableComponent>()) {
		clickable->onClick = [this]() {
			// 通知卡槽管理器这个卡牌被点击了
			auto manager = GetCardSlotManager();
			if (!IsReady() || !manager->CanAfford(mSunCost)) {
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CLICKFAILED, 0.5f);
				return;
			}
			manager->SelectCard(this->GetGameObject());
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CLICKSEED, 0.5f);
			};
	}
}

void CardComponent::Update() {
	if (mIsInChooseCardUI) return;
	// 更新冷却
	if (mIsCooldown) {
		mCooldownTimer -= DeltaTime::GetDeltaTime();
		if (mCooldownTimer <= 0) {
			mIsCooldown = false;
			mCooldownTimer = 0;
			// 冷却结束，强制更新状态
			ForceStateUpdate();
		}
		else {
			// 更新冷却进度显示
			if (auto display = GetCardDisplayComponent()) {
				float progress = 1.0f - (mCooldownTimer / mCooldownTime);
				display->SetCooldownProgress(progress);
			}
		}
	}
}

void CardComponent::ForceStateUpdate() {
	if (auto display = GetCardDisplayComponent()) {
		// 如果正在冷却，只更新冷却进度，不改变状态
		if (mIsCooldown) {
			float progress = 1.0f - (mCooldownTimer / mCooldownTime);
			display->SetCooldownProgress(progress);
		}
		else {
			// 不在冷却状态时，才根据阳光条件更新状态
			if (GetCardSlotManager()->CanAfford(mSunCost)) {
				display->TranToReady();
			}
			else {
				display->TranToWaitingSun();
			}
		}
	}
}

void CardComponent::StartCooldown() {
	if (IsReady() && !mIsCooldown) {
		mIsCooldown = true;
		mCooldownTimer = mCooldownTime;

		// 通知显示组件开始冷却
		if (auto display = GetCardDisplayComponent()) {
			display->TranToCooling();
		}
	}
}

void CardComponent::SetSelected(bool selected) {
	mIsSelected = selected;

	// 通知显示组件选中状态变化
	if (auto display = GetCardDisplayComponent()) {
		display->SetSelected(selected);
		if (selected) {
			display->TranToClick();
		}
		else {
			// 根据实际状态恢复
			if (IsReady()) {
				display->TranToReady();
			}
			else if (mIsCooldown) {
				display->TranToCooling();
			}
			else {
				display->TranToWaitingSun();
			}
		}
	}
}

float CardComponent::GetCooldownProgress() const {
	if (!mIsCooldown) return 1.0f;
	return 1.0f - (mCooldownTimer / mCooldownTime);
}

CardState CardComponent::GetCardState() const {
	if (auto display = GetCardDisplayComponent()) {
		return display->GetCardState();
	}
	return CardState::Cooling; // 默认返回冷却状态
}

std::shared_ptr<CardSlotManager> CardComponent::FindCardSlotManager() const {
	// 在场景中查找CardSlotManager
	auto& manager = GameObjectManager::GetInstance();
	auto allObjects = manager.GetAllGameObjects();

	for (size_t i = 0; i < allObjects.size(); i++)
	{
		if (auto gameObj = allObjects[i])
		{
			if (auto cardManager = gameObj->GetComponent<CardSlotManager>())
			{
				return cardManager;
			}
		}
	}

	std::cerr << "Warning: No CardSlotManager found in scene!" << std::endl;
	return nullptr;
}

std::shared_ptr<CardSlotManager> CardComponent::GetCardSlotManager() const {
	if (auto manager = mCardSlotManager.lock()) {
		return manager;
	}
	// 如果 weak_ptr 已失效，重新查找
	std::cerr << "Warning: CardSlotManager weak_ptr expired, re-finding..." << std::endl;
	auto manager = FindCardSlotManager();
	if (manager) {
		this->mCardSlotManager = manager;
	}
	return manager;
}

std::shared_ptr<CardDisplayComponent> CardComponent::GetCardDisplayComponent() const {
	if (auto display = mCardDisplayComponent.lock()) {
		return display;
	}
	// 如果 weak_ptr 已失效，重新获取
	if (auto gameObject = GetGameObject()) {
		if (auto display = gameObject->GetComponent<CardDisplayComponent>()) {
			this->mCardDisplayComponent = display;
			return display;
		}
	}
	return nullptr;
}
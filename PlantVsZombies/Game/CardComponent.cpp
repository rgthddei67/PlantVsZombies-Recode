#include "CardComponent.h"
#include "../Logger.h"
#include "../ResourceKeys.h"
#include "CardDisplayComponent.h"
#include "ClickableComponent.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./CardSlotManager.h"
#include "../DeltaTime.h"
#include "./ChooseCardUI.h"
#include "AudioSystem.h"
#include "../GameAPP.h"

CardComponent::CardComponent(PlantType type, int cost, float cooldown)
	: mPlantType(type), mSunCost(cost), mCooldownTime(cooldown)
{
	mCooldownTimer = 0.0f;
}

void CardComponent::Start() {
	if (auto* host = FindCardSlotManagerHost()) {
		mCardSlotManagerHost = host;
	}

	auto* gameObject = GetGameObject();
	auto* card = dynamic_cast<Card*>(gameObject);
	if (!card) return;

	mIsInChooseCardUI = card->GetIsInChooseCardUI();
	if (mIsInChooseCardUI) {
		SetCardChooseClick(card, card);
	}
	else
	{
		SetCardGameClick(card);
	}
}

void CardComponent::SetCardChooseClick(GameObject* gameObject, Card* card)
{
	if (!gameObject || !card) return;
	if (auto* clickable = gameObject->GetComponent<ClickableComponent>()) {
		// ---------- 选卡界面点击逻辑 ----------
		clickable->onClick = [card]() {
			if (card->IsMoving()) return;

			// 查找 ChooseCardUI
			auto& manager = GameObjectManager::GetInstance();
			ChooseCardUI* chooseUI = nullptr;
			for (const auto& obj : manager.GetAllGameObjects()) {
				if (obj && obj->GetName() == "ChooseCardUI") {
					chooseUI = dynamic_cast<ChooseCardUI*>(obj.get());
					break;
				}
			}

			if (!chooseUI) {
				LOG_ERROR("CardComponent") << "ChooseCardUI not found!";
				return;
			}

			// 切换选中状态
			chooseUI->ToggleCardSelection(card);
			};
	}
}

void CardComponent::SetCardGameClick(GameObject* gameObject)
{
	if (!gameObject) return;
	// 这里CardDisplayComponent还没有创建好，所以不能缓存 放到GetCardDisplayComponent里加载
	// 获取点击组件并设置回调
	if (auto* clickable = gameObject->GetComponent<ClickableComponent>()) {
		clickable->onClick = [this]() {
			// 通知卡槽管理器这个卡牌被点击了
			auto* manager = GetCardSlotManager();
			if (!IsReady() || !manager || !manager->CanAfford(mSunCost)) {
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
		// 开发者作弊：无冷却——已在冷却中的卡立即清零（双条件守卫，无 -develop 恒不生效）
		if (GameAPP::mDevelopMode && GameAPP::mDevNoCooldown) {
			mIsCooldown = false;
			mCooldownTimer = 0;
			ForceStateUpdate();
			return;
		}
		// 卡片加速只改变倒计时推进速度，不改写基础冷却或读档中的剩余秒数。
		double rechargeMultiplier = 1.0;
		if (auto* manager = GetCardSlotManager()) {
			if (auto* board = manager->GetBoard())
				rechargeMultiplier = board->GetPerkManager().GetPlantCardRechargeMultiplier();
		}
		mCooldownTimer -= static_cast<float>(DeltaTime::GetDeltaTime() * rechargeMultiplier);
		if (mCooldownTimer <= 0) {
			mIsCooldown = false;
			mCooldownTimer = 0;
			// 冷却结束，强制更新状态
			ForceStateUpdate();
		}
		else {
			// 更新冷却进度显示
			if (auto* display = GetCardDisplayComponent()) {
				float progress = 1.0f - (mCooldownTimer / mCooldownTime);
				display->SetCooldownProgress(progress);
			}
		}
	}
}

void CardComponent::ForceStateUpdate() {
	if (auto* display = GetCardDisplayComponent()) {
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

void CardComponent::RestoreCooldown(float timer, float time) {
	mCooldownTime = time;
	mCooldownTimer = timer;
	mIsCooldown = (timer > 0.0f);
	if (auto* display = GetCardDisplayComponent()) {
		if (mIsCooldown) {
			display->TranToCooling();
			display->SetCooldownProgress(1.0f - (mCooldownTimer / mCooldownTime));
		}
	}
}

void CardComponent::StartCooldown() {
	if (GameAPP::mDevelopMode && GameAPP::mDevNoCooldown) return;   // 开发者作弊：不进入冷却
	if (IsReady() && !mIsCooldown) {
		mIsCooldown = true;
		mCooldownTimer = mCooldownTime;

		// 通知显示组件开始冷却
		if (auto* display = GetCardDisplayComponent()) {
			display->TranToCooling();
		}
	}
}

void CardComponent::SetSelected(bool selected) {
	mIsSelected = selected;

	// 通知显示组件选中状态变化
	if (auto* display = GetCardDisplayComponent()) {
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
	if (auto* display = GetCardDisplayComponent()) {
		return display->GetCardState();
	}
	return CardState::Cooling; // 默认返回冷却状态
}

GameObject* CardComponent::FindCardSlotManagerHost() const {
	// 在场景中查找 CardSlotManager 所在的 GameObject
	auto& manager = GameObjectManager::GetInstance();
	const auto& allObjects = manager.GetAllGameObjects();

	for (size_t i = 0; i < allObjects.size(); i++)
	{
		if (const auto& gameObj = allObjects[i])
		{
			if (gameObj->GetComponent<CardSlotManager>())
			{
				return gameObj.get();
			}
		}
	}
	// LOG_WARN("CardComponent") << "No CardSlotManager found in scene!";
	return nullptr;
}

CardSlotManager* CardComponent::GetCardSlotManager() const {
	if (mCardSlotManagerHost) {
		if (auto* manager = mCardSlotManagerHost->GetComponent<CardSlotManager>()) {
			return manager;
		}
	}
	// 缓存失效，重新查找
	LOG_WARN("CardComponent") << "CardSlotManager host invalid, re-finding...";
	auto* host = FindCardSlotManagerHost();
	if (host) {
		this->mCardSlotManagerHost = host;
		return host->GetComponent<CardSlotManager>();
	}
	return nullptr;
}

CardDisplayComponent* CardComponent::GetCardDisplayComponent() const {
	if (mCardDisplayComponent) {
		return mCardDisplayComponent;
	}
	// 首次访问时从同 GameObject 获取并缓存
	if (auto* gameObject = GetGameObject()) {
		mCardDisplayComponent = gameObject->GetComponent<CardDisplayComponent>();
	}
	return mCardDisplayComponent;
}

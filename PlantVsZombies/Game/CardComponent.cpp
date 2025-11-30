#include "CardComponent.h"
#include "CardDisplayComponent.h"
#include "ClickableComponent.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./CardSlotManager.h"
#include "../DeltaTime.h"
#include "AudioSystem.h"

CardComponent::CardComponent(PlantType type, int cost, float cooldown)
    : mPlantType(type), mSunCost(cost), mCooldownTime(cooldown) {
	mCooldownTimer = 0.0f;
}

void CardComponent::Start() {
    // 获取点击组件并设置回调
    if (auto gameObject = GetGameObject()) {
        if (auto clickable = gameObject->GetComponent<ClickableComponent>()) {
            clickable->onClick = [this]() {
                // 通知卡槽管理器这个卡牌被点击了
                if (auto manager = FindCardSlotManager()) {
                    if (!IsReady() || !manager->CanAfford(mSunCost)) {
                        AudioSystem::PlaySound(AudioConstants::SOUND_CLICK_FAILED, 0.5f);
                        return;
					}
                    auto card = std::static_pointer_cast<GameObject>(GetGameObject()->shared_from_this());
                    manager->SelectCard(card);
					AudioSystem::PlaySound(AudioConstants::SOUND_CLICK_SEED, 0.5f);
                }
                };
        }
    }
}

void CardComponent::Update() {
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
            if (auto display = GetGameObject()->GetComponent<CardDisplayComponent>()) {
                float progress = 1.0f - (mCooldownTimer / mCooldownTime);
                display->SetCooldownProgress(progress);
            }
        }
    }
}

void CardComponent::ForceStateUpdate() {
    if (auto display = GetGameObject()->GetComponent<CardDisplayComponent>()) {
        // 如果正在冷却，只更新冷却进度，不改变状态
        if (mIsCooldown) {
            float progress = 1.0f - (mCooldownTimer / mCooldownTime);
            display->SetCooldownProgress(progress);
        }
        else {
            // 不在冷却状态时，才根据阳光条件更新状态
            if (auto manager = FindCardSlotManager()) {
                if (manager->CanAfford(mSunCost)) {
                    display->TranToReady();
                }
                else {
                    display->TranToWaitingSun();
                }
            }
            else {
                display->TranToReady(); // 默认就绪状态
            }
        }
    }
}

void CardComponent::StartCooldown() {
    if (IsReady() && !mIsCooldown) {
        mIsCooldown = true;
        mCooldownTimer = mCooldownTime;

        // 通知显示组件开始冷却
        if (auto display = GetGameObject()->GetComponent<CardDisplayComponent>()) {
            display->TranToCooling();
        }
    }
}

void CardComponent::SetSelected(bool selected) {
    mIsSelected = selected;

    // 通知显示组件选中状态变化
    if (auto display = GetGameObject()->GetComponent<CardDisplayComponent>()) {
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
    if (auto display = GetGameObject()->GetComponent<CardDisplayComponent>()) {
        return display->GetCardState();
    }
    return CardState::Cooling; // 默认返回冷却状态
}

std::shared_ptr<CardSlotManager> CardComponent::FindCardSlotManager() const {
    // 在场景中查找CardSlotManager
    auto& manager = GameObjectManager::GetInstance();
    auto allObjects = manager.GetAllGameObjects();

    for (auto& obj : allObjects) {
        if (auto cardManager = obj->GetComponent<CardSlotManager>()) {
            return cardManager;
        }
    }

    std::cerr << "Warning: No CardSlotManager found in scene!" << std::endl;
    return nullptr;
}
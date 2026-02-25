#include "CardDisplayComponent.h"
#include "../ResourceKeys.h"
#include "./Card.h"
#include "./CardComponent.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./TransformComponent.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include "../GameApp.h"
#include "./CardSlotManager.h"
#include "./Plant/GameDataManager.h"
#include <iostream>

CardDisplayComponent::CardDisplayComponent(PlantType type, int sunCost, float cooldown)
    : plantType(type), needSun(sunCost), cooldownTime(cooldown) {
    cardState = CardState::Ready;
    showMask = false;
    maskFillAmount = 0.0f;
    currentCooldown = cooldownTime; // 设置为满冷却，表示不需要冷却
}

void CardDisplayComponent::Start() {
    LoadTextures();
}

void CardDisplayComponent::Update() {
    if (GetCardComponent()->GetIsInChooseCardUI()) return;

    // 更新冷却计时
    if (cardState == CardState::Cooling) {
        currentCooldown += DeltaTime::GetDeltaTime();
        if (currentCooldown > cooldownTime) {
            currentCooldown = cooldownTime;
        }
    }

    // 更新状态
    UpdateCardState();
}

void CardDisplayComponent::Draw(Graphics* g) {
    if (!GetGameObject() || !GetGameObject()->IsActive()) return;
    auto transform = GetTransformComponent();
    if (!transform) return;

    DrawCardBackground(g, transform);
    DrawPlantImage(g, transform);

    if (showMask && maskFillAmount > 0 ||
        !GetCardComponent()->GetIsInChooseCardUI()) {
        DrawCooldownMask(g, transform);
    }

    DrawSunCost(g, transform);

    if (isSelected) {
        DrawSelectionHighlight(g, transform);
    }
}

void CardDisplayComponent::LoadTextures() {
    auto& resourceManager = ResourceManager::GetInstance();

    // 加载卡牌背景纹理（返回 const GLTexture*）
    cardBackground = resourceManager.GetTexture(ResourceKeys::Textures::IMAGE_CARD_BK);
    cardNormal = resourceManager.GetTexture(ResourceKeys::Textures::IMAGE_SEEDPACKETNORMAL);

    // 加载植物纹理
    std::string plantKey = GetPlantTextureKey();
    plantTexture = resourceManager.GetTexture(plantKey);

    if (!cardBackground) {
        std::cerr << "Failed to load card background texture" << std::endl;
    }
    if (!cardNormal) {
        std::cerr << "Failed to load card normal texture" << std::endl;
    }
    if (!plantTexture) {
        std::cerr << "Failed to load plant texture: " << plantKey << std::endl;
    }
}

void CardDisplayComponent::DrawCardBackground(Graphics* g, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject()) return;

    Vector pos = transform->GetPosition();
    Vector position = g->ScreenToWorldPosition(pos.x, pos.y);
    glm::vec4 color = GetCurrentColor();

    // 绘制卡牌背景
    if (cardNormal) {
        g->DrawTexture(cardNormal,
            position.x, position.y,
            static_cast<float>(CARD_WIDTH),
            static_cast<float>(CARD_HEIGHT),
            0.0f, color);
    }
}

void CardDisplayComponent::DrawPlantImage(Graphics* g, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject() || !plantTexture) return;

    Vector pos = transform->GetPosition();
    Vector position = g->ScreenToWorldPosition(pos.x, pos.y);

    int imgWidth = plantTexture->width;
    int imgHeight = plantTexture->height;

    // 植物图片位置（在卡牌中央）
    float drawX = position.x - 14;
    float drawY = position.y - 10;
    float drawW = imgWidth * 0.7f;
    float drawH = imgHeight * 0.7f;

    glm::vec4 color = GetCurrentColor();

    g->DrawTexture(plantTexture, drawX, drawY, drawW, drawH, 0.0f, color);
}

void CardDisplayComponent::DrawCooldownMask(Graphics* g, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject() || !cardBackground) return;

    Vector pos = transform->GetPosition();
    Vector position = g->ScreenToWorldPosition(pos.x, pos.y);

    // 计算遮罩高度（从顶部开始）
    int maskHeight = static_cast<int>(CARD_HEIGHT * maskFillAmount);
    SDL_Rect maskRect = {
        static_cast<int>(position.x),
        static_cast<int>(position.y),
        CARD_WIDTH,
        maskHeight
    };

    // 保存当前混合模式，设置为半透明混合
    BlendMode oldMode = g->GetBlendMode();
    g->SetBlendMode(BlendMode::Alpha);

    // 绘制半透明黑色矩形
    g->FillRect(static_cast<float>(maskRect.x),
        static_cast<float>(maskRect.y),
        static_cast<float>(maskRect.w),
        static_cast<float>(maskRect.h),
        glm::vec4(0.0f, 0.0f, 0.0f, 64.0f / 255.0f));

    // 恢复混合模式
    g->SetBlendMode(oldMode);
}

void CardDisplayComponent::DrawSunCost(Graphics* g, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject()) return;

    Vector pos = transform->GetPosition();
    Vector position = g->ScreenToWorldPosition(pos.x, pos.y);

    GameAPP::GetInstance().DrawText(std::to_string(needSun),
        Vector(position.x + 6, position.y + 58),
        { 0, 0, 0, 255 }, 
        ResourceKeys::Fonts::FONT_FZCQ,
        14);
}

void CardDisplayComponent::DrawSelectionHighlight(Graphics* g, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject()) return;

    Vector pos = transform->GetPosition();
    Vector position = g->ScreenToWorldPosition(pos.x, pos.y);

    BlendMode oldMode = g->GetBlendMode();
    g->SetBlendMode(BlendMode::Alpha);

    // 绘制半透明黑色遮罩表示选中
    g->FillRect(position.x, position.y,
        static_cast<float>(CARD_WIDTH),
        static_cast<float>(CARD_HEIGHT),
        glm::vec4(0.0f, 0.0f, 0.0f, 64.0f / 255.0f));

    g->SetBlendMode(oldMode);
}

void CardDisplayComponent::UpdateCardState() {
    // 获取卡牌逻辑组件
    auto cardComp = GetCardComponent();
    if (!cardComp) return;
    // 获取卡槽管理器
    auto cardSlotManager = cardComp->GetCardSlotManager();
    if (!cardSlotManager) return;

    // 如果正在冷却，只更新冷却进度，不进行状态转换
    if (cardState == CardState::Cooling) {
        // 更新冷却进度
        if (cardComp->IsCooldown()) {
            float progress = 1.0f - (cardComp->GetCooldownProgress());
            maskFillAmount = progress;
        }
        else {
            // 冷却结束，根据阳光条件转换状态
            if (cardSlotManager->CanAfford(needSun)) {
                TranToReady();
            }
            else {
                TranToWaitingSun();
            }
        }
        return;
    }

    // 获取当前阳光数量
    int currentSun = cardSlotManager->GetCurrentSun();

    // 根据条件更新状态（只处理非冷却状态）
    switch (cardState) {
    case CardState::Ready:
        // 就绪状态，检查阳光是否不足
        if (currentSun < needSun) {
            TranToWaitingSun();
        }
        break;

    case CardState::WaitingSun:
        // 等待阳光状态，检查阳光是否足够
        if (currentSun >= needSun) {
            TranToReady();
        }
        break;

    case CardState::Click:
        break;
    }
}

void CardDisplayComponent::UpdateCooldown(float deltaTime) {
    if (currentCooldown < cooldownTime) {
        currentCooldown += deltaTime;
        maskFillAmount = 1.0f - (currentCooldown / cooldownTime);

        if (currentCooldown >= cooldownTime) {
            TranToWaitingSun(); // 冷却结束，转为等待阳光状态
        }
    }
}

void CardDisplayComponent::TranToWaitingSun() {
    cardState = CardState::WaitingSun;
    showMask = true;
    maskFillAmount = 1.0f;
    currentCooldown = cooldownTime; // 确保冷却完成
}

void CardDisplayComponent::TranToReady() {
    if (isSelected)
    {
        return;
    }
    cardState = CardState::Ready;
    showMask = false;
    maskFillAmount = 0.0f;
    currentCooldown = cooldownTime; // 重置冷却
}

void CardDisplayComponent::TranToCooling() {
    cardState = CardState::Cooling;
    showMask = true;
    maskFillAmount = 1.0f;
    currentCooldown = 0.0f;
}

void CardDisplayComponent::TranToClick() {
    cardState = CardState::Click;
    showMask = false;
}

glm::vec4 CardDisplayComponent::GetCurrentColor() const {
    switch (cardState) {
    case CardState::Ready:
        return readyColor;
    case CardState::Cooling:
        return disabledColor;
    case CardState::WaitingSun:
        return waitingSunColor;
    case CardState::Click:
        return clickColor;
    default:
        return readyColor;
    }
}

std::shared_ptr<CardComponent> CardDisplayComponent::GetCardComponent() const {
    if (auto cardComp = mCardComponent.lock()) {
        return cardComp;
    }
    // 如果 weak_ptr 已失效，重新查找
    if (auto gameObject = GetGameObject()) {
        auto cardComp = gameObject->GetComponent<CardComponent>();
        if (cardComp) {
            this->mCardComponent = cardComp;
        }
        return cardComp;
    }
    return nullptr;
}

std::shared_ptr<TransformComponent> CardDisplayComponent::GetTransformComponent() const {
    if (auto transformComp = mTransformComponent.lock()) {
        return transformComp;
    }

    if (auto gameObject = GetGameObject()) {
        auto transformComp = gameObject->GetComponent<TransformComponent>();
        if (transformComp) {
            this->mTransformComponent = transformComp;
        }
        return transformComp;
    }
    return nullptr;
}

std::string CardDisplayComponent::GetPlantTextureKey() const {
    return GameDataManager::GetInstance().GetPlantTextureKey(plantType);
}
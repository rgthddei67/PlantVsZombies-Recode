#include "CardDisplayComponent.h"
#include "./Card.h"
#include "./CardComponent.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./TransformComponent.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include "../GameApp.h"
#include "./CardSlotManager.h"
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

void CardDisplayComponent::Draw(SDL_Renderer* renderer) {
    if (!GetGameObject() || !GetGameObject()->IsActive()) return;
    auto transfrom = GetTransformComponent();
	if (!transfrom) return;

    DrawCardBackground(renderer, transfrom);
    DrawPlantImage(renderer, transfrom);

    if (showMask && maskFillAmount > 0) {
        DrawCooldownMask(renderer, transfrom);
    }

    DrawSunCost(renderer, transfrom);

    if (isSelected) {
        DrawSelectionHighlight(renderer, transfrom);
    }
}

void CardDisplayComponent::LoadTextures() {
    auto& resourceManager = ResourceManager::GetInstance();

    // 加载卡牌背景纹理
    cardBackground = resourceManager.GetTexture("IMAGE_card_bk");
    cardNormal = resourceManager.GetTexture("IMAGE_SeedPacketNormal");

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

void CardDisplayComponent::DrawCardBackground(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject()) return;

	Vector position = transform->position;
    SDL_Rect cardRect = {
        static_cast<int>(position.x),
        static_cast<int>(position.y),
        CARD_WIDTH,  // 卡牌宽度
        CARD_HEIGHT   // 卡牌高度
    };

    // 绘制卡牌背景
    if (cardNormal) {
        // 根据状态调整背景纹理的颜色
        SDL_Color color = GetCurrentColor();
        SDL_SetTextureColorMod(cardNormal, color.r, color.g, color.b);
        SDL_RenderCopy(renderer, cardNormal, nullptr, &cardRect);
        // 重置纹理颜色
        SDL_SetTextureColorMod(cardNormal, 255, 255, 255);
    }
}

void CardDisplayComponent::DrawPlantImage(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject() || !plantTexture) return;

    Vector position = transform->position;

    // 植物图片位置（在卡牌中央）
    SDL_Rect plantRect = {
        static_cast<int>(position.x + 8),
        static_cast<int>(position.y + 13),
        40,  // 植物图片大小
        40
    };

    // 设置颜色调制（根据状态改变颜色）
    SDL_Color color = GetCurrentColor();
    SDL_SetTextureColorMod(plantTexture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(plantTexture, 255);

    SDL_RenderCopy(renderer, plantTexture, nullptr, &plantRect);

    // 重置颜色调制
    SDL_SetTextureColorMod(plantTexture, 255, 255, 255);
    SDL_SetTextureAlphaMod(plantTexture, 255);
}

void CardDisplayComponent::DrawCooldownMask(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject() || !cardBackground) return;

    Vector position = transform->position;
    SDL_Rect cardRect = {
        static_cast<int>(position.x),
        static_cast<int>(position.y),
        CARD_WIDTH,  // 卡牌宽度
        CARD_HEIGHT   // 卡牌高度
    };

    // 计算遮罩高度  - 从下往上逐渐减少
    // maskFillAmount 表示剩余冷却的比例 (1.0 = 完全冷却, 0.0 = 冷却完成)
    int maskHeight = static_cast<int>(cardRect.h * maskFillAmount);
    SDL_Rect maskRect = {
        cardRect.x,
        cardRect.y, // 从顶部开始
        cardRect.w,
        maskHeight  // 高度逐渐减少
    };

    // 保存当前的混合模式
    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);

    // 设置混合模式为混合（SDL_BLENDMODE_BLEND）
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 绘制半透明黑色矩形作为遮罩
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
    SDL_RenderFillRect(renderer, &maskRect);

    // 恢复之前的混合模式
    SDL_SetRenderDrawBlendMode(renderer, oldBlendMode);

}

void CardDisplayComponent::DrawSunCost(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject()) return;

    Vector position = transform->position;

    GameAPP::GetInstance().DrawText(renderer, std::to_string(needSun),
        Vector(position.x + 6, position.y + 58),
        { 0, 0, 0, 255 },
        "./font/fzcq.ttf",
        14);
}

void CardDisplayComponent::DrawSelectionHighlight(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform) {
    if (!GetGameObject()) return;

    Vector position = transform->position;

    // 绘制选中高亮边框
    SDL_Rect highlightRect = {
        static_cast<int>(position.x),
        static_cast<int>(position.y),
        CARD_WIDTH,  // 卡牌宽度
        CARD_HEIGHT   // 卡牌高度
    };

    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);

    // 设置混合模式为混合（SDL_BLENDMODE_BLEND）
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 绘制半透明黑色矩形作为遮罩
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
    SDL_RenderFillRect(renderer, &highlightRect);

    // 恢复之前的混合模式
    SDL_SetRenderDrawBlendMode(renderer, oldBlendMode);
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

SDL_Color CardDisplayComponent::GetCurrentColor() const {
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

// TODO 新增植物这里要改
std::string CardDisplayComponent::GetPlantTextureKey() const {
    switch (plantType) {
    case PlantType::PLANT_PEASHOOTER:
        return "IMAGE_PeaShooter";
    case PlantType::PLANT_SUNFLOWER:
        return "IMAGE_SunFlower";
    case PlantType::PLANT_WALLNUT:
        return "IMAGE_WallNut";
    case PlantType::PLANT_CHERRYBOMB:
        return "IMAGE_CherryBomb";
    case PlantType::PLANT_POTATOMINE:
        return "IMAGE_PotatoMine";
	case PlantType::PLANT_SNOWPEA:
		return "IMAGE_SnowPeaShooter";
    case PlantType::PLANT_CHOMPER:
		return "IMAGE_Chomper";
	case PlantType::PLANT_REPEATER:
		return "IMAGE_Repeater";
    default:
        return "IMAGE_PLANT_DEFAULT";
    }
}
#pragma once
#ifndef _CARD_DISPLAY_COMPONENT_H
#define _CARD_DISPLAY_COMPONENT_H

#include "Component.h"
#include "./Plant/PlantType.h"
#include <SDL2/SDL.h>
#include <string>

// 卡牌状态枚举
enum class CardState
{
    Cooling = 0,
    Ready = 1,
    WaitingSun = 2,
    Click = 3,
};

class CardComponent;
class TransformComponent;

class CardDisplayComponent : public Component {
private:
    // 纹理资源
    SDL_Texture* cardBackground = nullptr;    // IMAGE_card_bk
    SDL_Texture* cardNormal = nullptr;        // IMAGE_SeedPacketNormal
    SDL_Texture* plantTexture = nullptr;      // 植物图片

	mutable std::weak_ptr<CardComponent> mCardComponent;
	mutable std::weak_ptr<TransformComponent> mTransformComponent;

    // 状态相关
    CardState cardState = CardState::Cooling;
    PlantType plantType;
    int needSun = 0;
    float cooldownTime = 0;
    float currentCooldown = 0;
    bool isSelected = false;

    // 冷却遮罩相关
    float maskFillAmount = 1.0f;
    bool showMask = true;

    // 颜色状态
    SDL_Color readyColor = { 255, 255, 255, 255 };      // 白色 - 就绪
    SDL_Color disabledColor = { 160, 160, 160, 255 };   // 灰色 - 禁用/冷却
    SDL_Color waitingSunColor = { 215, 215, 215, 255 }; // 浅灰色 - 等待阳光
    SDL_Color clickColor = { 160, 160, 160, 255 };         // 深灰色 - 点击状态

public:
    CardDisplayComponent(PlantType type, int sunCost, float cooldown);

    void Start() override;
    void Update() override;
    void Draw(SDL_Renderer* renderer) override;

    // 状态转换
    void TranToWaitingSun();
    void TranToReady();
    void TranToCooling();
    void TranToClick();

    // 状态更新
    void UpdateCardState();
    void UpdateCooldown(float deltaTime);

    // 设置状态
    void SetSelected(bool selected) { isSelected = selected; }
    void SetCooldownProgress(float progress) { maskFillAmount = progress; }
    void SetNeedSun(int sun) { needSun = sun; }

    // 获取状态
    int GetNeedSun() const { return needSun; }
    PlantType GetPlantType() const { return plantType; }
    CardState GetCardState() const { return cardState; }
    bool IsReady() const { return cardState == CardState::Ready; }
    bool IsCooling() const { return cardState == CardState::Cooling; }

	std::shared_ptr<CardComponent> GetCardComponent() const;
	std::shared_ptr<TransformComponent> GetTransformComponent() const;

private:
    void LoadTextures();
    void DrawCardBackground(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform);
    void DrawPlantImage(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform);
    void DrawCooldownMask(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform);
    void DrawSunCost(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform);
    void DrawSelectionHighlight(SDL_Renderer* renderer, std::shared_ptr<TransformComponent> transform);

    SDL_Color GetCurrentColor() const;
    std::string GetPlantTextureKey() const;
};

#endif
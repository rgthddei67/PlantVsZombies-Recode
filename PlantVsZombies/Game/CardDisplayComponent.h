#pragma once
#ifndef _CARD_DISPLAY_COMPONENT_H
#define _CARD_DISPLAY_COMPONENT_H

#include "Component.h"
#include "./Plant/PlantType.h"
#include "../Graphics.h"          // 引入 Graphics
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
    // 纹理资源（改为 const GLTexture*）
    const GLTexture* cardBackground = nullptr;    // IMAGE_card_bk
    const GLTexture* cardNormal = nullptr;        // IMAGE_SeedPacketNormal
    const GLTexture* plantTexture = nullptr;      // 植物图片

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

    // 颜色状态（改为 glm::vec4，0~1范围）
    glm::vec4 readyColor = glm::vec4(1.0f);               // 白色
    glm::vec4 disabledColor = glm::vec4(0.627f, 0.627f, 0.627f, 1.0f); // 灰色
    glm::vec4 waitingSunColor = glm::vec4(0.843f, 0.843f, 0.843f, 1.0f); // 浅灰
    glm::vec4 clickColor = glm::vec4(0.627f, 0.627f, 0.627f, 1.0f); // 灰色

public:
    CardDisplayComponent(PlantType type, int sunCost, float cooldown);

    void Start() override;
    void Update() override;
    void Draw(Graphics* g) override;

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
    void DrawCardBackground(Graphics* g, std::shared_ptr<TransformComponent> transform);
    void DrawPlantImage(Graphics* g, std::shared_ptr<TransformComponent> transform);
    void DrawCooldownMask(Graphics* g, std::shared_ptr<TransformComponent> transform);
    void DrawSunCost(Graphics* g, std::shared_ptr<TransformComponent> transform);
    void DrawSelectionHighlight(Graphics* g, std::shared_ptr<TransformComponent> transform);

    glm::vec4 GetCurrentColor() const;
    std::string GetPlantTextureKey() const;
};

#endif
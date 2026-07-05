#pragma once
#ifndef _CARD_DISPLAY_COMPONENT_H
#define _CARD_DISPLAY_COMPONENT_H

#include "Component.h"
#include "./Plant/PlantType.h"
#include "./Definit.h"
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
	// 纹理资源（改为 const Texture*）
	const Texture* cardBackground = nullptr;    // IMAGE_card_bk
	const Texture* cardNormal = nullptr;        // IMAGE_SeedPacketNormal
	const Texture* plantTexture = nullptr;      // 植物图片

	// 同 GameObject 内的兄弟组件，生命周期一致，直接缓存裸指针
	mutable CardComponent* mCardComponent = nullptr;
	mutable TransformComponent* mTransformComponent = nullptr;

	// 状态相关
	CardState cardState = CardState::Cooling;
	PlantType plantType = PlantType::NUM_PLANT_TYPES;
	int needSun = 0;
	float cooldownTime = 0;
	float currentCooldown = 0;
	bool isSelected = false;

	// 冷却遮罩相关
	float maskFillAmount = 1.0f;
	bool showMask = true;

	// 颜色状态（改为 glm::vec4，0~255范围）
	glm::vec4 readyColor = glm::vec4(255.0f);               // 白色
	glm::vec4 disabledColor = glm::vec4(160.0f, 160.0f, 160.0f, 255.0f); // 灰色
	glm::vec4 waitingSunColor = glm::vec4(160.0f, 160.0f, 160.0f, 255.0f); // 浅灰
	glm::vec4 clickColor = glm::vec4(160.0f, 160.0f, 160.0f, 255.0f); // 灰色

	// 阳光数字文本缓存（避免每帧走 DrawText 的 key 构造与 LRU 维护）
	CachedText mSunTextCache{};
	int mCachedSunValue = -1;

public:
	CardDisplayComponent(PlantType type, int sunCost, float cooldown);

	void Start() override;
	void Update() override;
	bool NeedsUpdate() const override { return true; }
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

	CardComponent* GetCardComponent() const;
	TransformComponent* GetTransformComponent() const;

private:
	void LoadTextures();
	void DrawCardBackground(Graphics* g, const Vector& position, const glm::vec4& color);
	void DrawPlantImage(Graphics* g, const Vector& position, const glm::vec4& color);
	void DrawCooldownMask(Graphics* g, const Vector& position);
	void DrawSunCost(Graphics* g, const Vector& position);
	void DrawSelectionHighlight(Graphics* g, const Vector& position);

	glm::vec4 GetCurrentColor() const;
	std::string GetPlantTextureKey() const;
};

#endif

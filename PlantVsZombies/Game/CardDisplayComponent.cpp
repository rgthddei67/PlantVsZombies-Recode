#include "CardDisplayComponent.h"
#include "../Logger.h"
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

CardDisplayComponent::CardDisplayComponent(PlantType type, int sunCost, float cooldown)
	: plantType(type), needSun(sunCost), cooldownTime(cooldown) {
	cardState = CardState::Ready;
	showMask = false;
	maskFillAmount = 0.0f;
	currentCooldown = cooldownTime; // 设置为满冷却，表示不需要冷却
}

void CardDisplayComponent::Start() {
	LoadTextures();

	// 阳光数字纹理必须在主线程创建：AcquireTextTexture 内部走 glTexImage2D
	// 且写共享缓存，而卡片 Draw 现在跑在 worker 线程（并行录制），不能在那里
	// 建纹理。needSun 构造时即固定，这里主线程预热一次，DrawSunCost 之后永远命中缓存。
	mSunTextCache = GameAPP::GetInstance().GetGraphics().AcquireTextTexture(
		std::to_string(needSun),
		ResourceKeys::Fonts::FONT_FZCQ,
		14,
		glm::vec4(0.0f, 0.0f, 0.0f, 255.0f));
	mCachedSunValue = needSun;
}

void CardDisplayComponent::Update() {
	// 阳光数字纹理在主线程维护：数值变化、尚未创建、或 letterbox 切换（全屏⇄窗口）使旧句柄
	// 失效时重建。必须放在任何 early-return 之前，且只能在主线程做——卡片 Draw 跑在 worker
	// 线程，那里 AcquireTextTexture 直接返回空句柄。IsCachedTextStale 捕获全屏切换后旧密度/
	// 已销毁纹理，使数字按新分辨率重新光栅化以保持锐利。
	{
		Graphics& g = GameAPP::GetInstance().GetGraphics();
		if (mCachedSunValue != needSun || mSunTextCache.textureID == 0 || g.IsCachedTextStale(mSunTextCache)) {
			mSunTextCache = g.AcquireTextTexture(std::to_string(needSun),
				ResourceKeys::Fonts::FONT_FZCQ,
				14,
				glm::vec4(0.0f, 0.0f, 0.0f, 255.0f));
			mCachedSunValue = needSun;
		}
	}

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

	// 一次性算出世界坐标与当前色调，避免各子函数重复查询
	Vector pos = transform->GetPosition();
	Vector position = g->LogicalToWorld(pos.x, pos.y);
	glm::vec4 color = GetCurrentColor();

	DrawCardBackground(g, position, color);
	DrawPlantImage(g, position, color);

	if (showMask && maskFillAmount > 0) {
		DrawCooldownMask(g, position);
	}

	DrawSunCost(g, position);

	if (isSelected) {
		DrawSelectionHighlight(g, position);
	}
}

void CardDisplayComponent::LoadTextures() {
	auto& resourceManager = ResourceManager::GetInstance();

	// 加载卡牌背景纹理（返回 const Texture*）
	cardBackground = resourceManager.GetTexture(ResourceKeys::Textures::IMAGE_CARD_BK);
	cardNormal = resourceManager.GetTexture(ResourceKeys::Textures::IMAGE_SEEDPACKETNORMAL);

	// 加载植物纹理
	std::string plantKey = GetPlantTextureKey();
	plantTexture = resourceManager.GetTexture(plantKey);

	if (!cardBackground) {
		LOG_ERROR("CardDisplayComponent") << "Failed to load card background texture";
	}
	if (!cardNormal) {
		LOG_ERROR("CardDisplayComponent") << "Failed to load card normal texture";
	}
	if (!plantTexture) {
		LOG_ERROR("CardDisplayComponent") << "Failed to load plant texture: " << plantKey;
	}
}

void CardDisplayComponent::DrawCardBackground(Graphics* g, const Vector& position, const glm::vec4& color) {
	if (!cardNormal) return;
	g->DrawTexture(cardNormal,
		position.x, position.y,
		static_cast<float>(CARD_WIDTH),
		static_cast<float>(CARD_HEIGHT),
		0.0f, color);
}

void CardDisplayComponent::DrawPlantImage(Graphics* g, const Vector& position, const glm::vec4& color) {
	if (!plantTexture) return;

	float drawX = position.x - 14;
	float drawY = position.y - 10;
	float drawW = plantTexture->width * 0.7f;
	float drawH = plantTexture->height * 0.7f;

	g->DrawTexture(plantTexture, drawX, drawY, drawW, drawH, 0.0f, color);
}

void CardDisplayComponent::DrawCooldownMask(Graphics* g, const Vector& position) {
	if (!cardBackground) return;

	int maskHeight = static_cast<int>(CARD_HEIGHT * (1.0 - maskFillAmount));
	g->FillRect(position.x, position.y,
		static_cast<float>(CARD_WIDTH),
		static_cast<float>(maskHeight),
		glm::vec4(0.0f, 0.0f, 0.0f, 64.0f));
}

void CardDisplayComponent::DrawSunCost(Graphics* g, const Vector& position) {
	// 纹理的创建/重建已移到主线程 Update()（worker 线程不能建纹理）；这里只负责绘制。
	// 句柄若过期，DrawCachedText 内部会安全丢弃，等下一帧 Update 重建。
	g->DrawCachedText(mSunTextCache, position.x + 6, position.y + 58);
}

void CardDisplayComponent::DrawSelectionHighlight(Graphics* g, const Vector& position) {
	g->FillRect(position.x, position.y,
		static_cast<float>(CARD_WIDTH),
		static_cast<float>(CARD_HEIGHT),
		glm::vec4(0.0f, 0.0f, 0.0f, 64.0f));
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

CardComponent* CardDisplayComponent::GetCardComponent() const {
	if (mCardComponent) return mCardComponent;
	if (auto* gameObject = GetGameObject()) {
		mCardComponent = gameObject->GetComponent<CardComponent>();
	}
	return mCardComponent;
}

TransformComponent* CardDisplayComponent::GetTransformComponent() const {
	if (mTransformComponent) return mTransformComponent;
	if (auto* gameObject = GetGameObject()) {
		mTransformComponent = gameObject->GetComponent<TransformComponent>();
	}
	return mTransformComponent;
}

std::string CardDisplayComponent::GetPlantTextureKey() const {
	return GameDataManager::GetInstance().GetPlantTextureKey(plantType);
}
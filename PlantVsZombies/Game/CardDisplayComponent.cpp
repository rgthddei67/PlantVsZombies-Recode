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
#include "../GameAPP.h"
#include "./CardSlotManager.h"
#include "./Plant/GameDataManager.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr float kCardPlantImageScale = 0.64f;  // 普通植物卡图相对原始贴图的统一绘制倍率
	constexpr float kTallNutCardImageScale = 0.70f;  // 高坚果卡图在统一倍率之上的独立缩放
	constexpr float kTallNutCardImageOffsetY = -5.0f;  // 高坚果卡图上移量，避开底部阳光文字，单位：px
	constexpr float kBloverCardImageScale = 0.90f;  // 三叶草卡槽贴图在普通卡图基础上的独立缩放
}

CardDisplayComponent::CardDisplayComponent(PlantType type, int sunCost, float cooldown)
	: plantType(type), needSun(sunCost), cooldownTime(cooldown) {
	// cardState(Ready)/showMask(false)/maskFillAmount(0.0f) 均由头文件就地初始化
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
		kSunTextRasterSize,
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
				kSunTextRasterSize,
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
	Board* board = nullptr;
	if (CardComponent* component = GetCardComponent()) {
		if (CardSlotManager* manager = component->GetCardSlotManager()) {
			board = manager->GetBoard();
		}
	}
	const bool isActivePlantern = plantType == PlantType::PLANT_PLANTERN
		&& board && board->GetActivePlantern();
	glm::vec4 color = isActivePlantern ? readyColor : GetCurrentColor();

	DrawCardBackground(g, position, color);
	DrawPlantImage(g, position, color);

	if (!isActivePlantern && showMask && maskFillAmount > 0) {
		DrawCooldownMask(g, position);
	}

	if (isActivePlantern) DrawPlanternStatus(g, position);
	else DrawSunCost(g, position);
	if (plantType == PlantType::PLANT_BLOVER
		&& GetCardComponent() && !GetCardComponent()->GetIsInChooseCardUI()) {
		DrawBloverDirection(g, position);
	}

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

	const float typeScale = plantType == PlantType::PLANT_TALLNUT
		? kTallNutCardImageScale
		: plantType == PlantType::PLANT_BLOVER
			? kBloverCardImageScale
			: 1.0f;
	const float typeOffsetY = plantType == PlantType::PLANT_TALLNUT
		? kTallNutCardImageOffsetY
		: 0.0f;
	const float baseW = plantTexture->width * kCardPlantImageScale;
	const float baseH = plantTexture->height * kCardPlantImageScale;
	const float drawW = baseW * typeScale;
	const float drawH = baseH * typeScale;
	// 从既有卡图矩形中心缩放，避免缩小后向左上角漂移。
	const float drawX = position.x - 13.0f + (baseW - drawW) * 0.5f;
	const float drawY = position.y - 9.0f + (baseH - drawH) * 0.5f + typeOffsetY;

	const CardComponent* component = GetCardComponent();
	const bool flipBlover = plantType == PlantType::PLANT_BLOVER
		&& component && !component->GetIsInChooseCardUI()
		&& component->GetBloverDirection() == WindDirection::TOWARD_HOUSE;
	if (flipBlover) {
		// 方向箭头和卡图消费同一卡片状态；绕卡图中心镜像可保持 0.9 缩放后的占位不变。
		const float centerX = drawX + drawW * 0.5f;
		g->PushTransform();
		g->Translate(centerX, 0.0f);
		g->Scale(-1.0f, 1.0f);
		g->Translate(-centerX, 0.0f);
		g->DrawTexture(plantTexture, drawX, drawY, drawW, drawH, 0.0f, color);
		g->PopTransform();
		return;
	}

	g->DrawTexture(plantTexture, drawX, drawY, drawW, drawH, 0.0f, color);
}

void CardDisplayComponent::DrawPlanternStatus(Graphics* g, const Vector& position)
{
	CardComponent* component = GetCardComponent();
	CardSlotManager* manager = component ? component->GetCardSlotManager() : nullptr;
	Board* board = manager ? manager->GetBoard() : nullptr;
	if (!board) return;

	const float ratio = std::clamp(board->GetPlanternFuelRatio(), 0.0f, 1.0f);
	const float barX = position.x + 41.0f;
	const float barY = position.y + 7.0f;
	const float barW = 6.0f;
	const float barH = 43.0f;
	g->FillRect(barX - 1.0f, barY - 1.0f, barW + 2.0f, barH + 2.0f,
		glm::vec4(32.0f, 25.0f, 19.0f, 230.0f));
	const glm::vec4 fuelColor = ratio > 0.25f
		? glm::vec4(248.0f, 184.0f, 49.0f, 255.0f)
		: glm::vec4(238.0f, 92.0f, 45.0f, 255.0f);
	g->FillRect(barX, barY + barH * (1.0f - ratio),
		barW, barH * ratio, fuelColor);

	static const char* gearLabels[] = { "0", "I", "II", "III" };
	const int gear = std::clamp(board->GetPlanternGearValue(), 0, 3);
	g->DrawGlyphRun(gearLabels[gear], ResourceKeys::Fonts::FONT_FZCQ, 14,
		glm::vec4(50.0f, 28.0f, 12.0f, 255.0f),
		position.x + (gear < 2 ? 7.0f : 4.0f), position.y + 51.0f);

	const bool fullHint = board->GetPlanternFuelFullHintTimer() > 0.0f;
	const std::string fuelText = std::to_string(static_cast<int>(std::lround(
		board->GetPlanternFuel())));
	const float fuelTextX = position.x + (fuelText.size() >= 3 ? 17.0f : 21.0f);
	g->DrawGlyphRun(fuelText, ResourceKeys::Fonts::FONT_FZCQ, 12,
		fullHint
			? glm::vec4(172.0f, 72.0f, 12.0f, 255.0f)
			: glm::vec4(50.0f, 28.0f, 12.0f, 255.0f),
		fuelTextX, position.y + 52.0f);

	if (fullHint) {
		g->DrawRect(position.x + 1.0f, position.y + 1.0f,
			static_cast<float>(CARD_WIDTH - 2), static_cast<float>(CARD_HEIGHT - 2),
			glm::vec4(255.0f, 205.0f, 60.0f, 255.0f));
	}
}

void CardDisplayComponent::DrawBloverDirection(
	Graphics* g, const Vector& position)
{
	CardComponent* component = GetCardComponent();
	if (!g || !component) return;

	const bool towardFront =
		component->GetBloverDirection() == WindDirection::TOWARD_FRONT;
	const float tailX = position.x + (towardFront ? 31.0f : 45.0f);
	const float tipX = position.x + (towardFront ? 45.0f : 31.0f);
	const float centerY = position.y + 57.0f;
	const float headSign = towardFront ? -1.0f : 1.0f;
	const glm::vec4 back(25.0f, 36.0f, 25.0f, 220.0f);
	const glm::vec4 arrow(112.0f, 225.0f, 118.0f, 255.0f);
	g->FillRect(position.x + 28.0f, position.y + 50.0f,
		20.0f, 14.0f, back);
	g->DrawLine(tailX, centerY, tipX, centerY, arrow);
	g->DrawLine(tipX, centerY, tipX + headSign * 5.0f, centerY - 4.0f, arrow);
	g->DrawLine(tipX, centerY, tipX + headSign * 5.0f, centerY + 4.0f, arrow);
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
	g->DrawCachedText(mSunTextCache, position.x + 5, position.y + 51, kSunTextDrawScale);
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
			// 冷却结束，同时检查阳光与植物的本关累计种植次数。
			if (cardSlotManager->CanUsePlant(plantType, needSun)) {
				TranToReady();
			}
			else {
				TranToWaitingSun();
			}
		}
		return;
	}

	// 阳光与植物次数统一走 CanUsePlant；其中阳光判断仍包含开发者“无视阳光”守卫，
	// 但每关数量上限不会被开发者免费种植绕过。
	const bool usable = cardSlotManager->CanUsePlant(plantType, needSun);

	// 根据条件更新状态（只处理非冷却状态）
	switch (cardState) {
	case CardState::Ready:
		// 就绪状态，检查阳光或本关种植次数是否不足。
		if (!usable) {
			TranToWaitingSun();
		}
		break;

	case CardState::WaitingSun:
		// 灰态复用既有 WaitingSun 视觉；阳光和次数任一恢复即可重新判断。
		if (usable) {
			TranToReady();
		}
		break;

	case CardState::Click:
		break;

	case CardState::Cooling:
		// 冷却状态由 UpdateCooldown 单独处理，此处有意不处理
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

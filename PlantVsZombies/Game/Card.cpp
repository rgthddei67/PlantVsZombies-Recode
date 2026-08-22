#include "Card.h"
#include "AudioSystem.h"
#include "CardSlotManager.h"
#include "ChooseCardUI.h"
#include "ClickableComponent.h"
#include "ColliderComponent.h"
#include "GameObjectManager.h"
#include "./Plant/GameDataManager.h"
#include "./Plant/Plantern.h"
#include "./Plant/PlantUpgradeRules.h"
#include "../DeltaTime.h"
#include "../GameApp.h"
#include "../Logger.h"
#include "../ResourceKeys.h"
#include "../ResourceManager.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr float kCardPlantImageScale = 0.64f; // 普通植物卡图相对原始贴图的统一绘制倍率
	constexpr float kTallNutCardImageScale = 0.70f; // 高坚果卡图在统一倍率之上的独立缩放
	constexpr float kTallNutCardImageOffsetY = -5.0f; // 高坚果卡图上移量，单位：px
	constexpr float kBloverCardImageScale = 0.90f; // 三叶草卡槽贴图独立缩放
	constexpr float kMelonPultCardImageScale = 0.80f; // 西瓜投手卡图独立缩放
	constexpr float kMelonPultCardImageOffsetX = -10.0f; // 西瓜投手卡图水平视觉修正，单位：UI px
	constexpr float kMelonPultCardImageOffsetY = 1.0f; // 西瓜投手卡图垂直视觉修正，单位：UI px
	constexpr float kCobCannonCardImageScale = 0.60f; // 双格炮卡图独立缩放
	constexpr float kCobCannonCardImageOffsetX = -20.0f; // 双格炮卡图水平视觉修正，单位：UI px
	constexpr float kUpgradeCardSourceX = 50.0f; // seeds.png 紫卡底板源 X，单位：纹理 px
	constexpr float kUpgradeCardSourceY = 0.0f; // seeds.png 紫卡底板源 Y，单位：纹理 px
	constexpr float kUpgradeCardSourceWidth = 50.0f; // 紫卡底板源宽度，单位：纹理 px
	constexpr float kUpgradeCardSourceHeight = 70.0f; // 紫卡底板源高度，单位：纹理 px
	constexpr float kPlanternLowFuelPulseSpeed = 8.0f; // 低燃料描边每未缩放秒的脉冲相位速度
	constexpr float kPlanternGearLabelAreaWidth = 20.0f; // 路灯花挡位标签布局宽度，单位：UI px
	constexpr int kPlanternGearLabelFontSize = 14; // 路灯花挡位标签字号，单位：逻辑 px
}

Card::Card(PlantType plantType, int sunCost, float cooldown, bool isInChooseCardUI)
	: mPlantType(plantType),
	  mSunCost(sunCost),
	  mCooldownTime(cooldown),
	  mIsInChooseCardUI(isInChooseCardUI)
{
	mObjectType = ObjectType::OBJECT_UI;
	SetupComponents();
}

/** 创建卡片的空间值以及仍待后续阶段迁移的碰撞、点击附件。 */
void Card::SetupComponents()
{
	CreateTransform();
	auto* collision = AddComponent<ColliderComponent>(Vector(CARD_WIDTH, CARD_HEIGHT));
	collision->isStatic = true;
	collision->isTrigger = true;
	collision->layerMask = CollisionLayer::NONE;
	collision->collisionMask = CollisionLayer::NONE;
	auto* clickable = AddComponent<ClickableComponent>();
	clickable->ConsumeEvent = true;

	SetName("PlantCard");
	SetTag("Card");
}

/** 在主线程启动附件、纹理缓存和当前上下文的点击入口。 */
void Card::Start()
{
	GameObject::Start();
	LoadTextures();
	UpdateSunTextCache();
	ConfigureClickHandler();
}

void Card::SetIsInChooseCardUI(bool isInChooseCardUI)
{
	mIsInChooseCardUI = isInChooseCardUI;
	if (mStarted) ConfigureClickHandler();
}

/** 根据当前上下文安装唯一点击回调。 */
void Card::ConfigureClickHandler()
{
	if (mIsInChooseCardUI) SetCardChooseClick();
	else SetCardGameClick();
}

void Card::SetCardChooseClick()
{
	auto* clickable = GetComponent<ClickableComponent>();
	if (!clickable) return;

	clickable->onClick = [this]() {
		if (IsMoving()) return;

		ChooseCardUI* chooseUI = nullptr;
		for (const auto& object : GameObjectManager::GetInstance().GetAllGameObjects()) {
			if (object && object->GetName() == "ChooseCardUI") {
				chooseUI = dynamic_cast<ChooseCardUI*>(object.get());
				break;
			}
		}
		if (!chooseUI) {
			LOG_ERROR("Card") << "ChooseCardUI not found!";
			return;
		}
		chooseUI->ToggleCardSelection(this);
	};
}

void Card::SetCardGameClick()
{
	auto* clickable = GetComponent<ClickableComponent>();
	if (!clickable) return;

	clickable->onClick = [this]() {
		auto* manager = GetCardSlotManager();
		if (!manager || !manager->CanAcceptGameplayInput()) return;
		if (mPlantType == PlantType::PLANT_PLANTERN
			&& manager->GetBoard() && manager->GetBoard()->GetActivePlantern()) {
			manager->TogglePlanternGearMenu();
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CLICKSEED, 0.5f);
			return;
		}
		if (!IsReady() || !manager->CanUsePlant(mPlantType, mSunCost)) {
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CLICKFAILED, 0.5f);
			return;
		}
		manager->SelectCard(this);
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CLICKSEED, 0.5f);
	};
}

/** 保持原来的“显示状态→玩法冷却→Clickable→移动”更新顺序。 */
void Card::Update()
{
	if (mActive && mStarted) {
		UpdateSunTextCache();
		UpdateVisualState();
		UpdateCooldown();
	}
	GameObject::Update();
	UpdateMovement();
}

/** 只在主线程创建或刷新阳光数字纹理，worker Draw 仅消费缓存。 */
void Card::UpdateSunTextCache()
{
	Graphics& graphics = GameAPP::GetInstance().GetGraphics();
	if (mCachedSunValue == mSunCost && mSunTextCache.BindingId() != 0
		&& !graphics.IsCachedTextStale(mSunTextCache)) {
		return;
	}
	mSunTextCache = graphics.AcquireTextTexture(std::to_string(mSunCost),
		ResourceKeys::Fonts::FONT_FZCQ, kSunTextRasterSize,
		glm::vec4(0.0f, 0.0f, 0.0f, 255.0f));
	mCachedSunValue = mSunCost;
}

/** 根据 Card 的权威状态更新灰态、冷却遮罩与可用性。 */
void Card::UpdateVisualState()
{
	if (mIsInChooseCardUI) return;
	auto* manager = GetCardSlotManager();
	if (!manager) return;

	if (mVisualState == VisualState::Cooling) {
		if (mIsCooldown) {
			mMaskFillAmount = 1.0f - GetCooldownProgress();
		}
		else if (manager->CanUsePlant(mPlantType, mSunCost)) {
			TransitionToReady();
		}
		else {
			TransitionToWaitingSun();
		}
		return;
	}

	const bool usable = manager->CanUsePlant(mPlantType, mSunCost);
	switch (mVisualState) {
	case VisualState::Ready:
		if (!usable) TransitionToWaitingSun();
		break;
	case VisualState::WaitingSun:
		if (usable) TransitionToReady();
		break;
	case VisualState::Click:
	case VisualState::Cooling:
		break;
	}
}

/** 推进实战冷却；倍率只作用于倒计时速度，不改基础时长与存档剩余值。 */
void Card::UpdateCooldown()
{
	if (mIsInChooseCardUI || !mIsCooldown) return;

	if (GameAPP::mDevelopMode && GameAPP::mDevNoCooldown) {
		mIsCooldown = false;
		mCooldownTimer = 0.0f;
		ForceStateUpdate();
		return;
	}

	double rechargeMultiplier = 1.0;
	if (auto* manager = GetCardSlotManager()) {
		if (auto* board = manager->GetBoard()) {
			rechargeMultiplier = board->GetPerkManager().GetPlantCardRechargeMultiplier();
		}
	}
	mCooldownTimer -= static_cast<float>(DeltaTime::GetDeltaTime() * rechargeMultiplier);
	if (mCooldownTimer <= 0.0f) {
		mIsCooldown = false;
		mCooldownTimer = 0.0f;
		ForceStateUpdate();
		return;
	}
	mMaskFillAmount = GetCooldownProgress();
}

void Card::UpdateMovement()
{
	if (!mIsMoving || !GetTransform()) return;

	const Vector currentPos = GetTransform()->GetPosition();
	const Vector dir = mTargetPos - currentPos;
	const float dist = dir.magnitude();
	if (dist < 0.1f) {
		GetTransform()->SetPosition(mTargetPos);
		mIsMoving = false;
		return;
	}

	const float step = mMoveSpeed * DeltaTime::GetDeltaTime();
	if (step >= dist) {
		GetTransform()->SetPosition(mTargetPos);
		mIsMoving = false;
		return;
	}
	GetTransform()->SetPosition(currentPos + dir.normalized() * step);
}

void Card::ForceStateUpdate()
{
	if (mIsCooldown) {
		mMaskFillAmount = GetCooldownProgress();
		return;
	}

	auto* manager = GetCardSlotManager();
	if (manager && manager->CanUsePlant(mPlantType, mSunCost)) {
		TransitionToReady();
	}
	else {
		TransitionToWaitingSun();
	}
}

void Card::RestoreCooldown(float timer, float time)
{
	mCooldownTime = time;
	mCooldownTimer = timer;
	mIsCooldown = timer > 0.0f;
	if (mIsCooldown) {
		TransitionToCooling();
		mMaskFillAmount = GetCooldownProgress();
	}
}

void Card::SetBloverDirection(WindDirection direction)
{
	if (direction != WindDirection::TOWARD_HOUSE
		&& direction != WindDirection::TOWARD_FRONT) {
		return;
	}
	mBloverDirection = direction;
}

void Card::ToggleBloverDirection()
{
	if (mPlantType != PlantType::PLANT_BLOVER) return;
	mBloverDirection = mBloverDirection == WindDirection::TOWARD_HOUSE
		? WindDirection::TOWARD_FRONT : WindDirection::TOWARD_HOUSE;
}

void Card::BindCardSlotManager(CardSlotManager* manager)
{
	mCardSlotManager = manager;
	if (mCardSlotManager && mStarted && !mIsInChooseCardUI) {
		ForceStateUpdate();
	}
}

void Card::StartCooldown()
{
	if (GameAPP::mDevelopMode && GameAPP::mDevNoCooldown) return;
	if (!IsReady() || mIsCooldown) return;

	mIsCooldown = true;
	mCooldownTimer = mCooldownTime;
	TransitionToCooling();
}

void Card::SetSelected(bool selected)
{
	mIsSelected = selected;
	if (selected) {
		TransitionToClick();
	}
	else if (IsReady()) {
		TransitionToReady();
	}
	else if (mIsCooldown) {
		TransitionToCooling();
	}
	else {
		TransitionToWaitingSun();
	}
}

float Card::GetCooldownProgress() const
{
	if (!mIsCooldown || mCooldownTime <= 0.0f) return 1.0f;
	return 1.0f - (mCooldownTimer / mCooldownTime);
}

void Card::SetTargetPosition(const Vector& target)
{
	mTargetPos = target;
	mIsMoving = true;
}

void Card::SnapToOriginalPosition()
{
	if (GetTransform()) GetTransform()->SetPosition(mOriginalPos);
	mTargetPos = mOriginalPos;
	mIsMoving = false;
}

void Card::TransitionToWaitingSun()
{
	mVisualState = VisualState::WaitingSun;
	mShowMask = true;
	mMaskFillAmount = 1.0f;
}

void Card::TransitionToReady()
{
	if (mIsSelected) return;
	mVisualState = VisualState::Ready;
	mShowMask = false;
	mMaskFillAmount = 0.0f;
}

void Card::TransitionToCooling()
{
	mVisualState = VisualState::Cooling;
	mShowMask = true;
	mMaskFillAmount = 1.0f;
}

void Card::TransitionToClick()
{
	mVisualState = VisualState::Click;
	mShowMask = false;
}

glm::vec4 Card::GetCurrentColor() const
{
	switch (mVisualState) {
	case VisualState::Ready:
		return mReadyColor;
	case VisualState::Cooling:
		return mDisabledColor;
	case VisualState::WaitingSun:
		return mWaitingSunColor;
	case VisualState::Click:
		return mClickColor;
	}
	return mReadyColor;
}

/** 绘制 Card 专属视觉；保留基类组件的调试绘制先于卡面提交。 */
void Card::Draw(Graphics* g)
{
	if (!mActive || !mStarted || !g) return;
	GameObject::Draw(g);
	if (!GetTransform()) return;

	const Vector logical = GetTransform()->GetPosition();
	const Vector position = g->LogicalToWorld(logical.x, logical.y);
	Board* board = nullptr;
	if (mPlantType == PlantType::PLANT_PLANTERN && !mIsInChooseCardUI) {
		if (auto* manager = GetCardSlotManager()) board = manager->GetBoard();
	}
	const bool isActivePlantern = mPlantType == PlantType::PLANT_PLANTERN
		&& board && board->GetActivePlantern();
	const glm::vec4 color = isActivePlantern ? mReadyColor : GetCurrentColor();

	DrawCardBackground(g, position, color);
	DrawPlantImage(g, position, color);
	if (!isActivePlantern && mShowMask && mMaskFillAmount > 0.0f) {
		DrawCooldownMask(g, position);
	}
	if (isActivePlantern) DrawPlanternStatus(g, position);
	else DrawSunCost(g, position);
	if (mPlantType == PlantType::PLANT_BLOVER && !mIsInChooseCardUI) {
		DrawBloverDirection(g, position);
	}
	if (mIsSelected) DrawSelectionHighlight(g, position);
}

void Card::LoadTextures()
{
	auto& resources = ResourceManager::GetInstance();
	mCardBackground = resources.GetTexture(ResourceKeys::Textures::IMAGE_CARD_BK);
	mCardNormal = resources.GetTexture(ResourceKeys::Textures::IMAGE_SEEDPACKETNORMAL);
	mCardVariants = resources.GetTexture(ResourceKeys::Textures::IMAGE_SEEDPACKETVARIANTS);
	mPlantTexture = resources.GetTexture(GetPlantTextureKey());

	if (!mCardBackground) LOG_ERROR("Card") << "Failed to load card background texture";
	if (!mCardNormal) LOG_ERROR("Card") << "Failed to load card normal texture";
	if (!mCardVariants) LOG_ERROR("Card") << "Failed to load card variant texture";
	if (!mPlantTexture) {
		LOG_ERROR("Card") << "Failed to load plant texture: " << GetPlantTextureKey();
	}
}

void Card::DrawCardBackground(
	Graphics* g, const Vector& position, const glm::vec4& color)
{
	if (IsUpgradePlantType(mPlantType)) {
		if (!mCardVariants) return;
		g->DrawTextureRegion(mCardVariants,
			kUpgradeCardSourceX, kUpgradeCardSourceY,
			kUpgradeCardSourceWidth, kUpgradeCardSourceHeight,
			position.x, position.y,
			static_cast<float>(CARD_WIDTH), static_cast<float>(CARD_HEIGHT),
			0.0f, color);
		return;
	}
	if (!mCardNormal) return;
	g->DrawTexture(mCardNormal, position.x, position.y,
		static_cast<float>(CARD_WIDTH), static_cast<float>(CARD_HEIGHT),
		0.0f, color);
}

void Card::DrawPlantImage(
	Graphics* g, const Vector& position, const glm::vec4& color)
{
	if (!mPlantTexture) return;
	const bool isMelonFamily = mPlantType == PlantType::PLANT_MELONPULT
		|| mPlantType == PlantType::PLANT_WINTERMELON;
	const bool isCobCannon = mPlantType == PlantType::PLANT_COBCANNON;
	const float typeScale = mPlantType == PlantType::PLANT_TALLNUT
		? kTallNutCardImageScale
		: mPlantType == PlantType::PLANT_BLOVER
			? kBloverCardImageScale
			: isMelonFamily
				? kMelonPultCardImageScale
				: isCobCannon ? kCobCannonCardImageScale : 1.0f;
	const float typeOffsetX = isCobCannon
		? kCobCannonCardImageOffsetX
		: isMelonFamily ? kMelonPultCardImageOffsetX : 0.0f;
	const float typeOffsetY = mPlantType == PlantType::PLANT_TALLNUT
		? kTallNutCardImageOffsetY
		: isMelonFamily ? kMelonPultCardImageOffsetY : 0.0f;
	const float baseW = mPlantTexture->width * kCardPlantImageScale;
	const float baseH = mPlantTexture->height * kCardPlantImageScale;
	const float drawW = baseW * typeScale;
	const float drawH = baseH * typeScale;
	const float drawX = position.x - 13.0f + (baseW - drawW) * 0.5f + typeOffsetX;
	const float drawY = position.y - 9.0f + (baseH - drawH) * 0.5f + typeOffsetY;

	const bool flipBlover = mPlantType == PlantType::PLANT_BLOVER
		&& !mIsInChooseCardUI
		&& mBloverDirection == WindDirection::TOWARD_HOUSE;
	if (flipBlover) {
		const float centerX = drawX + drawW * 0.5f;
		g->PushTransform();
		g->Translate(centerX, 0.0f);
		g->Scale(-1.0f, 1.0f);
		g->Translate(-centerX, 0.0f);
		g->DrawTexture(mPlantTexture, drawX, drawY, drawW, drawH, 0.0f, color);
		g->PopTransform();
		return;
	}
	g->DrawTexture(mPlantTexture, drawX, drawY, drawW, drawH, 0.0f, color);
}

void Card::DrawPlanternStatus(Graphics* g, const Vector& position)
{
	auto* manager = GetCardSlotManager();
	Board* board = manager ? manager->GetBoard() : nullptr;
	if (!board) return;

	const float ratio = std::clamp(board->GetPlanternFuelRatio(), 0.0f, 1.0f);
	Plantern* plantern = board->GetActivePlantern();
	const bool lowFuel = board->SupportsPlanternMechanics()
		&& plantern && plantern->IsFuelLow();
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
	const float gearLabelWidth = g->MeasureTextWidth(gearLabels[gear],
		ResourceKeys::Fonts::FONT_FZCQ, kPlanternGearLabelFontSize);
	const float gearLabelX = position.x
		+ (kPlanternGearLabelAreaWidth - gearLabelWidth) * 0.5f;
	g->DrawGlyphRun(gearLabels[gear], ResourceKeys::Fonts::FONT_FZCQ,
		kPlanternGearLabelFontSize,
		glm::vec4(50.0f, 28.0f, 12.0f, 255.0f),
		gearLabelX, position.y + 51.0f);

	const bool fullHint = board->GetPlanternFuelFullHintTimer() > 0.0f;
	const std::string fuelText = std::to_string(static_cast<int>(std::lround(
		board->GetPlanternFuel())));
	const float fuelTextX = position.x + (fuelText.size() >= 3 ? 17.0f : 21.0f);
	g->DrawGlyphRun(fuelText, ResourceKeys::Fonts::FONT_FZCQ, 12,
		fullHint
			? glm::vec4(172.0f, 72.0f, 12.0f, 255.0f)
			: (lowFuel
				? glm::vec4(214.0f, 28.0f, 24.0f, 255.0f)
				: glm::vec4(50.0f, 28.0f, 12.0f, 255.0f)),
		fuelTextX, position.y + 52.0f);

	if (fullHint) {
		g->DrawRect(position.x + 1.0f, position.y + 1.0f,
			static_cast<float>(CARD_WIDTH - 2), static_cast<float>(CARD_HEIGHT - 2),
			glm::vec4(255.0f, 205.0f, 60.0f, 255.0f));
	}
	else if (lowFuel) {
		const float pulse = 0.5f + 0.5f
			* std::sin(static_cast<float>(DeltaTime::GetUnscaledTotalTime())
				* kPlanternLowFuelPulseSpeed);
		const float alpha = 150.0f + 105.0f * pulse;
		g->DrawRect(position.x, position.y,
			static_cast<float>(CARD_WIDTH), static_cast<float>(CARD_HEIGHT),
			glm::vec4(255.0f, 42.0f, 36.0f, alpha));
		g->DrawRect(position.x + 2.0f, position.y + 2.0f,
			static_cast<float>(CARD_WIDTH - 4), static_cast<float>(CARD_HEIGHT - 4),
			glm::vec4(255.0f, 86.0f, 42.0f, alpha * 0.72f));
	}
}

void Card::DrawBloverDirection(Graphics* g, const Vector& position)
{
	const bool towardFront = mBloverDirection == WindDirection::TOWARD_FRONT;
	const float tailX = position.x + (towardFront ? 31.0f : 45.0f);
	const float tipX = position.x + (towardFront ? 45.0f : 31.0f);
	const float centerY = position.y + 57.0f;
	const float headSign = towardFront ? -1.0f : 1.0f;
	const glm::vec4 back(25.0f, 36.0f, 25.0f, 220.0f);
	const glm::vec4 arrow(112.0f, 225.0f, 118.0f, 255.0f);
	g->FillRect(position.x + 28.0f, position.y + 50.0f, 20.0f, 14.0f, back);
	g->DrawLine(tailX, centerY, tipX, centerY, arrow);
	g->DrawLine(tipX, centerY, tipX + headSign * 5.0f, centerY - 4.0f, arrow);
	g->DrawLine(tipX, centerY, tipX + headSign * 5.0f, centerY + 4.0f, arrow);
}

void Card::DrawCooldownMask(Graphics* g, const Vector& position)
{
	if (!mCardBackground) return;
	const int maskHeight = static_cast<int>(CARD_HEIGHT * (1.0f - mMaskFillAmount));
	g->FillRect(position.x, position.y,
		static_cast<float>(CARD_WIDTH), static_cast<float>(maskHeight),
		glm::vec4(0.0f, 0.0f, 0.0f, 64.0f));
}

void Card::DrawSunCost(Graphics* g, const Vector& position)
{
	g->DrawCachedText(mSunTextCache, position.x + 5.0f, position.y + 51.0f,
		kSunTextDrawScale);
}

void Card::DrawSelectionHighlight(Graphics* g, const Vector& position)
{
	g->FillRect(position.x, position.y,
		static_cast<float>(CARD_WIDTH), static_cast<float>(CARD_HEIGHT),
		glm::vec4(0.0f, 0.0f, 0.0f, 64.0f));
}

std::string Card::GetPlantTextureKey() const
{
	return GameDataManager::GetInstance().GetPlantTextureKey(mPlantType);
}

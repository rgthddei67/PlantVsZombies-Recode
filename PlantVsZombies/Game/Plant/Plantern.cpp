#include "Plantern.h"

#include "../Board.h"
#include "../ClickableComponent.h"
#include "../CursorObjectManager.h"
#include "../ShadowComponent.h"
#include "../../DeltaTime.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr float kLowBurnRate = 0.5f;     // 一档每游戏秒消耗的雾火
	constexpr float kMediumBurnRate = 1.0f;  // 二档每游戏秒消耗的雾火
	constexpr float kHighBurnRate = 2.0f;    // 三档每游戏秒消耗的雾火
	constexpr float kFuelFullHintSeconds = 1.8f; // 雾火溢出后卡槽提示的持续游戏秒
}

void Plantern::SetupPlant()
{
	Plant::SetupPlant();
	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetScale(Vector(0.82f, 0.82f));
		shadow->SetOffset(Vector(0.0f, 29.0f));
	}
	if (mIsPreview) return;

	// 本体点击只发出展示请求；挡位按钮和菜单生命周期仍由卡槽 UI 持有。
	auto* clickable = AddComponent<ClickableComponent>();
	clickable->ConsumeEvent = true;
	clickable->onClick = [this]() {
		if (!mBoard || IsSquished()
			|| mBoard->mCursorObjectManager.GetActiveType() != CursorObjectType::NONE) {
			return;
		}
		mBoard->TogglePlanternGearMenu();
	};
}

void Plantern::PlantUpdate()
{
	const float deltaTime = DeltaTime::GetDeltaTime();
	mFuelFullHintTimer = std::max(0.0f, mFuelFullHintTimer - deltaTime);
	// 4-1 只承担迷雾视觉教学；从 4-2 起才启用燃料经济与索敌压力。
	if (!mBoard || !mBoard->SupportsPlanternMechanics()
		|| mGear == PlanternGear::OFF || mFuel <= 0.0f) {
		return;
	}

	mFuel = std::max(0.0f, mFuel - GetBurnRate(mGear) * deltaTime);
}

void Plantern::Draw(Graphics* g)
{
	if (g && HasUsableLight()) {
		const Vector center = GetVisualAnchorPosition() + Vector(0.0f, -9.0f);
		const float gear = static_cast<float>(static_cast<int>(mGear));
		const float pulse = 0.94f + 0.06f
			* std::sin(static_cast<float>(mBoard ? mBoard->mBoardFrame : 0) * 0.08f);
		const float radius = (24.0f + gear * 5.0f) * pulse;
		g->FillCircle(center.x, center.y, radius,
			glm::vec4(255.0f, 205.0f, 72.0f, 28.0f), 32);
		g->FillCircle(center.x, center.y, radius * 0.56f,
			glm::vec4(255.0f, 235.0f, 145.0f, 42.0f), 28);
	}
	Plant::Draw(g);
}

void Plantern::Die()
{
	if (mBoard) mBoard->NotifyPlanternRemoved(mPlantID);
	Plant::Die();
}

void Plantern::SaveExtraData(nlohmann::json& j) const
{
	j["fuel"] = mFuel;
	j["gear"] = static_cast<int>(mGear);
}

void Plantern::LoadExtraData(const nlohmann::json& j)
{
	SetFuel(j.value("fuel", INITIAL_FUEL));
	const int gear = std::clamp(j.value("gear", static_cast<int>(PlanternGear::LOW)),
		static_cast<int>(PlanternGear::OFF), static_cast<int>(PlanternGear::HIGH));
	mGear = static_cast<PlanternGear>(gear);
	mFuelFullHintTimer = 0.0f;
}

float Plantern::AddFuel(float amount)
{
	if (amount <= 0.0f) return 0.0f;
	const float accepted = std::min(amount, FUEL_CAPACITY - mFuel);
	mFuel += accepted;
	if (accepted + 0.001f < amount) {
		mFuelFullHintTimer = kFuelFullHintSeconds;
	}
	return accepted;
}

void Plantern::SetFuel(float fuel)
{
	mFuel = std::clamp(fuel, 0.0f, FUEL_CAPACITY);
}

void Plantern::SetGear(PlanternGear gear)
{
	const int value = std::clamp(static_cast<int>(gear),
		static_cast<int>(PlanternGear::OFF), static_cast<int>(PlanternGear::HIGH));
	mGear = static_cast<PlanternGear>(value);
}

float Plantern::GetBurnRate(PlanternGear gear)
{
	switch (gear) {
	case PlanternGear::OFF: return 0.0f;
	case PlanternGear::LOW: return kLowBurnRate;
	case PlanternGear::MEDIUM: return kMediumBurnRate;
	case PlanternGear::HIGH: return kHighBurnRate;
	}
	return 0.0f;
}

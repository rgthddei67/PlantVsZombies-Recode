#include "NorthStarFlower.h"

#include "../../DeltaTime.h"
#include "../../GameApp.h"
#include "../AudioSystem.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../../ResourceKeys.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kChargeDurationSeconds = 12.0f; // 从空能量蓄满一次领域所需的游戏秒
constexpr float kActiveDurationSeconds = 8.0f; // 一次导航领域持续的游戏秒
constexpr float kReadyPulseRate = 0.09f; // 就绪星芒随 Board 帧脉动的频率
}

void NorthStarFlower::SetupPlant()
{
	mPlantHealth = 300;
	mPlantMaxHealth = 300;
	if (auto* shadow = GetShadow()) {
		shadow->SetOffset(Vector(0.0f, 28.0f));
		shadow->SetScale(Vector(0.72f, 0.55f));
	}
}

void NorthStarFlower::PlantUpdate()
{
	if (mIsPreview) return;
	const float deltaTime = DeltaTime::GetDeltaTime();
	if (mActiveRemaining > 0.0f) {
		mActiveRemaining = std::max(0.0f, mActiveRemaining - deltaTime);
		return;
	}
	mChargeSeconds = std::min(kChargeDurationSeconds, mChargeSeconds + deltaTime);
}

bool NorthStarFlower::CoversPolarNavigationCell(int row, int column) const
{
	return IsActive() && !IsSquished() && std::abs(row - mRow) <= 1
		&& std::abs(column - mColumn) <= 1;
}

bool NorthStarFlower::IsPolarNavigationReady() const
{
	return IsActive() && !mIsPreview && !IsSquished() && mActiveRemaining <= 0.0f
		&& mChargeSeconds >= kChargeDurationSeconds;
}

bool NorthStarFlower::ActivatePolarNavigation()
{
	if (!IsPolarNavigationReady()) return false;
	mChargeSeconds = 0.0f;
	mActiveRemaining = kActiveDurationSeconds;
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BLEEP, 0.36f);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("NorthStarActivate", GetVisualAnchorPosition());
	}
	return true;
}

void NorthStarFlower::Draw(Graphics* g)
{
	if (g && mBoard && IsPolarNavigationActive()) {
		const int firstRow = std::max(0, mRow - 1);
		const int lastRow = std::min(mBoard->mRows - 1, mRow + 1);
		const int firstColumn = std::max(0, mColumn - 1);
		const int lastColumn = std::min(mBoard->mColumns - 1, mColumn + 1);
		const Vector topLeft = mBoard->GetCellCenterPosition(firstRow, firstColumn);
		const Vector bottomRight = mBoard->GetCellCenterPosition(lastRow, lastColumn);
		const float halfWidth = CELL_COLLIDER_SIZE_X * 0.5f;
		const float halfHeight = mBoard->GetCellHeight() * 0.5f;
		g->DrawRect(topLeft.x - halfWidth, topLeft.y - halfHeight,
			bottomRight.x - topLeft.x + CELL_COLLIDER_SIZE_X,
			bottomRight.y - topLeft.y + mBoard->GetCellHeight(),
			glm::vec4(98.0f, 224.0f, 255.0f, 150.0f));
	}
	Plant::Draw(g);
	if (!g) return;
	const Vector center = GetVisualAnchorPosition() + Vector(0.0f, -25.0f);
	const float pulse = 1.0f + 0.12f * std::sin(
		static_cast<float>(mBoard ? mBoard->mBoardFrame : 0) * kReadyPulseRate);
	const float radius = (IsPolarNavigationReady() || IsPolarNavigationActive())
		? 15.0f * pulse : 8.0f + 6.0f * (mChargeSeconds / kChargeDurationSeconds);
	g->DrawLine(center.x - radius, center.y, center.x + radius, center.y,
		glm::vec4(210.0f, 248.0f, 255.0f, 230.0f));
	g->DrawLine(center.x, center.y - radius, center.x, center.y + radius,
		glm::vec4(210.0f, 248.0f, 255.0f, 230.0f));
	g->FillCircle(center.x, center.y, 4.0f,
		glm::vec4(250.0f, 255.0f, 255.0f, 245.0f), 14);
}

void NorthStarFlower::SaveExtraData(nlohmann::json& j) const
{
	j["chargeSeconds"] = mChargeSeconds;
	j["activeRemaining"] = mActiveRemaining;
}

void NorthStarFlower::LoadExtraData(const nlohmann::json& j)
{
	mChargeSeconds = std::clamp(j.value("chargeSeconds", 0.0f),
		0.0f, kChargeDurationSeconds);
	mActiveRemaining = std::clamp(j.value("activeRemaining", 0.0f),
		0.0f, kActiveDurationSeconds);
}

#include "GroundRift.h"

#include "Board.h"
#include "GameObjectManager.h"
#include "Plant/Plant.h"
#include "../DeltaTime.h"
#include "../Graphics.h"
#include "../ParticleSystem/ParticleSystem.h"
#include "../ResourceKeys.h"
#include "../ResourceManager.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr float kRiftDrawOffsetX = -8.0f; // 贴图左端相对传播前沿的偏移，单位 px
	constexpr float kRiftGroundOffsetY = 43.0f; // 裂缝相对僵尸碰撞基线的向下偏移，单位 px
	constexpr float kRiftRemoveX = 58.0f; // 前沿越过房屋侧后的回收位置，单位世界 X
}

GroundRift::GroundRift(Board* board, int row, float frontX,
	int nextColumn, float downstreamDamageMultiplier)
	: GameObject(ObjectType::OBJECT_NONE)
	, mBoard(board)
	, mRow(row)
	, mNextColumn(nextColumn)
	, mDownstreamDamageMultiplier(std::clamp(
		downstreamDamageMultiplier, 0.0f, 1.0f))
{
	SetTag("GroundRift");
	SetName("GroundRift");
	CreateTransform(Vector(frontX, 0.0f));
}

float GroundRift::GetFrontX() const
{
	return GetTransform() ? GetTransform()->GetPosition().x : 0.0f;
}

void GroundRift::Update()
{
	GameObject::Update();
	if (!mBoard || !GetTransform() || !IsActive()) return;

	const float deltaTime = std::max(0.0f, DeltaTime::GetDeltaTime());
	const float newFrontX = GetFrontX() - kTravelSpeed * deltaTime;
	ResolveCrossedColumns(newFrontX);
	GetTransform()->SetPosition(Vector(newFrontX, 0.0f));
	if (mNextColumn < 0 && newFrontX <= kRiftRemoveX) Finish();
}

void GroundRift::Draw(Graphics* g)
{
	if (!g || !mBoard || !IsActive()) return;
	const Texture* texture = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_ICECRACKDRILLRIFT, false);
	if (!texture) return;
	const float frontX = GetFrontX();
	const float groundY = mBoard->GetZombieCollisionY(mRow, frontX)
		+ kRiftGroundOffsetY;
	g->DrawTexture(texture, frontX + kRiftDrawOffsetX,
		groundY - static_cast<float>(texture->height),
		static_cast<float>(texture->width),
		static_cast<float>(texture->height));
}

/** 跨倍速大步长时仍从右到左逐列结算，避免跳格或雪锚倍率应用顺序漂移。 */
void GroundRift::ResolveCrossedColumns(float newFrontX)
{
	if (!mBoard || mNextColumn < 0) return;
	while (mNextColumn >= 0) {
		const float columnCenterX = mBoard->GetCellCenterPosition(
			mRow, mNextColumn).x;
		if (newFrontX > columnCenterX) break;
		ResolveColumn(mNextColumn);
		--mNextColumn;
	}
}

void GroundRift::ResolveColumn(int column)
{
	if (!mBoard || column < 0 || column >= mBoard->mColumns) return;
	const int damage = std::max(1, static_cast<int>(std::lround(
		static_cast<float>(kPlantDamage) * mDownstreamDamageMultiplier)));
	const WinterGroundImpactResponse response =
		mBoard->ApplyWinterGroundImpactToCell(mRow, column,
			WinterGroundImpactKind::GROUND_CRACK, damage, DamageSource::ZOMBIE);
	// 雪锚等拦截只改变后续左侧格；当前格四层共享命中前的同一伤害倍率。
	if (response.intercepted) {
		mDownstreamDamageMultiplier = std::clamp(
			mDownstreamDamageMultiplier * response.downstreamDamageMultiplier,
			0.0f, 1.0f);
	}

	if (g_particleSystem) {
		const Vector center = mBoard->GetCellCenterPosition(mRow, column);
		g_particleSystem->EmitEffect("IceCrackDrillRift",
			Vector(center.x, mBoard->GetZombieCollisionY(mRow, center.x)
				+ kRiftGroundOffsetY - 8.0f));
	}
}

void GroundRift::Finish()
{
	if (!IsActive()) return;
	if (mBoard) mBoard->RemoveGroundRift(this);
	else {
		SetActive(false);
		GameObjectManager::GetInstance().DestroyGameObject(this);
	}
}

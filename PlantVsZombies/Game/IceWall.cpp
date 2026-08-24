#include "IceWall.h"

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
	constexpr float kWallMoveSpeed = 14.0f;              // 冰墙向房屋推进速度，单位 px/游戏秒
	constexpr float kPlantClearance = 8.0f;              // 墙左缘与最近植物碰撞箱之间保留的视觉间隙，单位 px
	constexpr float kThawDamagePerSecond = 120.0f;       // 回暖或温暖阶段每游戏秒融化的墙体生命
	constexpr float kWallDrawBottomOffsetY = 36.0f;      // 墙底相对僵尸逻辑碰撞基线的向下偏移，单位 px
	constexpr float kWallRemoveCenterX = 70.0f;           // 无植物阻挡时墙移出房屋侧后回收的中心 X
	constexpr float kProjectileAimHeight = 68.0f;        // 盐晶瞄准点相对墙底的向上高度，单位 px
}

IceWall::IceWall(Board* board, int row, float centerX,
	int health, int maxHealth, float thawDamageRemainder)
	: GameObject(ObjectType::OBJECT_NONE)
	, mBoard(board)
	, mRow(row)
	, mHealth(std::max(1, health))
	, mMaxHealth(std::max(1, maxHealth))
	, mThawDamageRemainder(std::clamp(thawDamageRemainder, 0.0f, 0.999999f))
{
	mHealth = std::min(mHealth, mMaxHealth);
	SetTag("IceWall");
	SetName("IceWall");
	CreateTransform(Vector(centerX, 0.0f));
}

float IceWall::GetCenterX() const
{
	return GetTransform() ? GetTransform()->GetPosition().x : 0.0f;
}

void IceWall::Update()
{
	GameObject::Update();
	if (!mBoard || !GetTransform() || !IsActive()) return;

	const float deltaTime = DeltaTime::GetDeltaTime();
	const bool thawing = mBoard->GetColdWavePhase() == ColdWavePhase::THAWING
		|| mBoard->GetAmbientTemperatureC() > mBoard->GetWinterFreezingTemperatureC();
	if (thawing && deltaTime > 0.0f) {
		mThawDamageRemainder += kThawDamagePerSecond * deltaTime;
		const int damage = static_cast<int>(std::floor(mThawDamageRemainder));
		if (damage > 0) {
			mThawDamageRemainder -= static_cast<float>(damage);
			ApplyDamage(damage, false);
			if (!IsActive()) return;
		}
	}

	const float currentX = GetCenterX();
	const float stopCenterX = FindPlantStopCenterX();
	const float nextX = std::max(stopCenterX, currentX - kWallMoveSpeed * deltaTime);
	GetTransform()->SetPosition(Vector(nextX, 0.0f));
	if (nextX <= kWallRemoveCenterX) Break();
}

void IceWall::Draw(Graphics* g)
{
	if (!g || !mBoard || !GetTransform() || !IsActive()) return;
	const Texture* texture = ResourceManager::GetInstance().GetTexture(
		GetTextureKey(), false);
	if (!texture) return;
	const float centerX = GetCenterX();
	const float bottomY = mBoard->GetZombieCollisionY(mRow, centerX)
		+ kWallDrawBottomOffsetY;
	g->DrawTexture(texture,
		centerX - static_cast<float>(texture->width) * 0.5f,
		bottomY - static_cast<float>(texture->height),
		static_cast<float>(texture->width),
		static_cast<float>(texture->height));
}

bool IceWall::IntersectsHorizontalSegment(float fromX, float toX) const
{
	if (!IsActive() || mHealth <= 0) return false;
	const float left = GetCenterX() - kBlockHalfWidth;
	const float right = GetCenterX() + kBlockHalfWidth;
	return std::max(fromX, toX) >= left && std::min(fromX, toX) <= right;
}

Vector IceWall::GetProjectileAimPosition() const
{
	if (!mBoard) return Vector(GetCenterX(), 0.0f);
	const float bottomY = mBoard->GetZombieCollisionY(mRow, GetCenterX())
		+ kWallDrawBottomOffsetY;
	return Vector(GetCenterX(), bottomY - kProjectileAimHeight);
}

int IceWall::TakeProjectileDamage(int damage, bool fireDamage)
{
	const int multiplier = fireDamage ? 2 : 1;
	return ApplyDamage(std::max(0, damage) * multiplier, true);
}

int IceWall::ApplyWinterCorrosion(int corrosion)
{
	// 盐晶的 20 点直接伤害已经播放墙体命中反馈，腐蚀段只追加数值结算。
	return ApplyDamage(std::max(0, corrosion), false);
}

void IceWall::SetStateForTesting(
	float centerX, int health, float thawDamageRemainder)
{
	if (GetTransform()) GetTransform()->SetPosition(Vector(centerX, 0.0f));
	mHealth = std::clamp(health, 1, mMaxHealth);
	mThawDamageRemainder = std::clamp(thawDamageRemainder, 0.0f, 0.999999f);
}

int IceWall::ApplyDamage(int damage, bool emitHitFeedback)
{
	if (!IsActive() || damage <= 0 || mHealth <= 0) return 0;
	const int applied = std::min(mHealth, damage);
	mHealth -= applied;
	if (emitHitFeedback && g_particleSystem) {
		g_particleSystem->EmitEffect("IceWallHit", GetProjectileAimPosition());
	}
	if (mHealth <= 0) Break();
	return applied;
}

void IceWall::Break()
{
	if (!IsActive()) return;
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("IceWallBreak", GetProjectileAimPosition());
	}
	if (mBoard) {
		mBoard->RemoveIceWall(this);
	}
	else {
		SetActive(false);
		GameObjectManager::GetInstance().DestroyGameObject(this);
	}
}

float IceWall::FindPlantStopCenterX() const
{
	if (!mBoard) return -10000.0f;
	float nearestPlantRight = -10000.0f;
	const float wallCenter = GetCenterX();
	for (const int plantID : mBoard->mEntityRegistry.GetAllPlantIDs()) {
		const Plant* plant = mBoard->mEntityRegistry.GetPlant(plantID);
		if (!plant || !plant->IsActive() || plant->IsPreview()
			|| plant->IsSquished() || plant->mRow != mRow) continue;
		const ColliderComponent* collider = plant->GetColliderComponent();
		if (!collider || !collider->mEnabled) continue;
		const SDL_FRect bounds = collider->GetBoundingBox();
		const float right = bounds.x + bounds.w;
		if (right <= wallCenter + kBlockHalfWidth) {
			nearestPlantRight = std::max(nearestPlantRight, right);
		}
	}
	return nearestPlantRight <= -9999.0f
		? -10000.0f
		: nearestPlantRight + kPlantClearance + kBlockHalfWidth;
}

const std::string& IceWall::GetTextureKey() const
{
	using namespace ResourceKeys::Textures;
	if (mHealth * 3 <= mMaxHealth) return IMAGE_ICE_WALL_CRACKED2;
	if (mHealth * 3 <= mMaxHealth * 2) return IMAGE_ICE_WALL_CRACKED1;
	return IMAGE_ICE_WALL;
}

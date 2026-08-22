#include "WallNut.h"
#include "../ShadowComponent.h"

#include "../../ParticleSystem/ParticleSystem.h"

#include <cstdint>

namespace {
	constexpr int kWallNutHealth = 4000;                 // 经典坚果墙基础生命值
	constexpr float kTextureRefreshSeconds = 0.5f;       // 裂纹阶段轮询间隔，单位：秒
	constexpr float kBiteParticleOffsetX = 30.0f;        // 啃食碎屑相对植物逻辑中心的迎击面水平偏移，单位：px
	constexpr float kBiteParticleOffsetY = -25.0f;       // 啃食碎屑相对植物逻辑中心的嘴部高度偏移，单位：px
}

void WallNut::SetupPlant()
{
	Plant::SetupPlant();
	this->mPlantHealth = kWallNutHealth;
	this->mPlantMaxHealth = kWallNutHealth;

	if (auto shadowComponent = GetShadow()) {
		shadowComponent->SetOffset(Vector(4, 26));
	}
}

void WallNut::PlantUpdate()
{
	// 被啃食时暂停动画，不再被啃食时恢复
	bool isBeingEaten = mEaterCount > 0;
	if (isBeingEaten && !mWasBeingEaten) {
		mAnimator->SetExtraSpeedMultiplier(0.0f);   // 被啃食：状态层暂停动画，不动 base
	}
	else if (!isBeingEaten && mWasBeingEaten) {
		mAnimator->SetExtraSpeedMultiplier(1.0f);   // 不再被啃：恢复
	}
	mWasBeingEaten = isBeingEaten;

	mUpdateTextureTimer += DeltaTime::GetDeltaTime();

	if (mUpdateTextureTimer >= kTextureRefreshSeconds) {
		mUpdateTextureTimer = 0.0f;
		UpdateTexture();
	}
}

void WallNut::OnZombieBite(const Vector& eaterPosition)
{
	if (!g_particleSystem) return;

	const float direction = eaterPosition.x >= GetPosition().x ? 1.0f : -1.0f;
	g_particleSystem->EmitEffect("WallnutEatSmall",
		GetPosition() + Vector(direction * kBiteParticleOffsetX, kBiteParticleOffsetY));
}

const std::string& WallNut::GetBodyTextureKey() const
{
	return ResourceKeys::Textures::IMAGE_WALLNUT_BODY;
}

const std::string& WallNut::GetCrackedTextureKey(int damageStage) const
{
	return damageStage >= 2
		? ResourceKeys::Textures::IMAGE_WALLNUT_CRACKED2
		: ResourceKeys::Textures::IMAGE_WALLNUT_CRACKED1;
}

void WallNut::UpdateTexture(bool emitParticle)
{
	const int nextStage = mPlantHealth <= mPlantMaxHealth / 3
		? 2
		: (mPlantHealth <= static_cast<int64_t>(mPlantMaxHealth) * 2 / 3 ? 1 : 0);
	if (nextStage == mDamageStage || !mAnimator) return;

	const std::string& textureKey = nextStage == 0
		? GetBodyTextureKey()
		: GetCrackedTextureKey(nextStage);
	const Texture* damageTexture = ResourceManager::GetInstance().GetTexture(textureKey);
	if (!damageTexture) return;

	mDamageStage = nextStage;
	mAnimator->SetTrackImage(GetDamageTrackName(), damageTexture);
	if (emitParticle && nextStage > 0 && g_particleSystem) {
		g_particleSystem->EmitEffect("WallnutEatLarge",
			GetPosition() + Vector(0.0f, GetCrackParticleOffsetY()));
	}
}

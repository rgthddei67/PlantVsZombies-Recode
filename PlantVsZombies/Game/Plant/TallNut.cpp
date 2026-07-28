#include "TallNut.h"

#include "../ShadowComponent.h"
#include "../../ParticleSystem/ParticleSystem.h"

namespace {
	constexpr int kTallNutHealth = 9000;                    // 经典高坚果基础生命值
	constexpr float kTallNutColliderWidth = 85.0f;          // 比普通植物向迎敌面多覆盖约 20 px
	constexpr float kTallNutShadowOffsetY = 29.0f;          // 高坚果脚底阴影相对逻辑中心的垂直偏移，单位：px
	constexpr float kTallNutShadowScaleX = 1.3f;            // 原版高坚果阴影横向放大倍率
	constexpr float kJumpBlockParticleOffsetX = 20.0f;      // 跳跃撞击星光相对逻辑中心的水平偏移，单位：px
	constexpr float kJumpBlockParticleOffsetY = -65.0f;     // 跳跃撞击星光相对逻辑中心的垂直偏移，单位：px
	constexpr int kTyphoonPlantImpactDamage = 800;           // 每直接挡住一个植物格前进一格所承受的环境伤害
}

void TallNut::SetupPlant()
{
	WallNut::SetupPlant();
	mPlantHealth = kTallNutHealth;
	mPlantMaxHealth = kTallNutHealth;

	if (mCollider) {
		// 保持普通植物左边缘，只把迎敌面的判定加宽，贴合原版 Tallnut PlantRect。
		mCollider->size.x = kTallNutColliderWidth;
	}
	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetOffset(Vector(4.0f, kTallNutShadowOffsetY));
		shadow->SetScale(Vector(kTallNutShadowScaleX, 0.75f));
	}
}

bool TallNut::BlocksZombieJump(ZombieJumpType jumpType) const
{
	return jumpType == ZombieJumpType::POLEVAULT
		|| jumpType == ZombieJumpType::DOLPHIN_RIDER;
}

void TallNut::OnZombieJumpBlocked(ZombieJumpType)
{
	PlayBlockFeedback();
}

void TallNut::OnTyphoonPlantImpact(bool showFeedback)
{
	// 伤害逐格立即结算，确保高坚果中途死亡后阵风的剩余步数可以重新读取空出的格位。
	if (showFeedback) PlayBlockFeedback();
	TakeDamage(kTyphoonPlantImpactDamage, DamageSource::OTHER);
}

void TallNut::PlayBlockFeedback()
{
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BONK, 0.5f);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("TallNutBlock",
			GetPosition() + Vector(kJumpBlockParticleOffsetX, kJumpBlockParticleOffsetY));
	}
}

const std::string& TallNut::GetBodyTextureKey() const
{
	return ResourceKeys::Textures::IMAGE_TALLNUT_BODY;
}

const std::string& TallNut::GetCrackedTextureKey(int damageStage) const
{
	return damageStage >= 2
		? ResourceKeys::Textures::IMAGE_TALLNUT_CRACKED2
		: ResourceKeys::Textures::IMAGE_TALLNUT_CRACKED1;
}

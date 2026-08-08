#include "Caltrop.h"

#include "../../ResourceKeys.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

#include <algorithm>

namespace {
	constexpr int kCaltropDamage = 20;                    // C# DoRowAreaDamage：每次攻击伤害
	constexpr float kAttackCycleSeconds = 0.99f;          // C# mStateCountdown=99：攻击周期，单位秒
	constexpr float kAttackAnimationSpeed = 1.5f;         // C# 18fps / Caltrop.reanim 12fps
	constexpr float kAttackRectFromCenterX = -20.0f;      // C# mX+20 换算到 80px 格子中心后的左缘
	constexpr float kAttackRectWidth = 30.0f;             // C# mWidth-50，原版植物宽度 80px

	float HorizontalOverlap(const SDL_FRect& rect, float left, float width)
	{
		return std::max(0.0f,
			std::min(rect.x + rect.w, left + width) - std::max(rect.x, left));
	}
}

void Caltrop::SetupPlant()
{
	// 原版地刺贴地绘制，不投射普通植物阴影。
	RemoveComponent<ShadowComponent>();

	if (mIsPreview || !mAnimator) return;

	// 主人给出的帧号已经是 AddFrameEvent 口径，直接使用 25；idle 活跃区间 0..20，
	// attack 活跃区间 21..30，因此该全局帧事件不会被待机轨误触发。
	mAnimator->AddFrameEvent(25, [this]() { DamageTargetsAtAttackFrame(); }, true);
}

void Caltrop::PlantUpdate()
{
	const float attackSpeed = GetAttackSpeedMultiplier();
	mAttackCooldown = std::max(0.0f,
		mAttackCooldown - DeltaTime::GetDeltaTime() * attackSpeed);
	if (mAttackCooldown <= 0.0f && HasTargetInAttackRect()) {
		StartAttack();
	}
}

bool Caltrop::HasTargetInAttackRect() const
{
	if (!mBoard) return false;

	const float attackLeft = GetPosition().x + kAttackRectFromCenterX;
	bool found = false;
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (found || !zombie || zombie->IsMindControlled() || zombie->IsDying()
			|| !zombie->CanBeTargetedByProjectile(false)) return;
		const ColliderComponent* collider = zombie->GetColliderComponent();
		if (collider && collider->mEnabled && HorizontalOverlap(collider->GetBoundingBox(),
			attackLeft, kAttackRectWidth) > 0.0f) {
			found = true;
		}
	});
	return found;
}

void Caltrop::DamageTargetsAtAttackFrame()
{
	if (!mBoard) return;

	const float attackLeft = GetPosition().x + kAttackRectFromCenterX;
	bool hitAny = false;
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (!zombie || zombie->IsMindControlled() || zombie->IsDying()
			|| !zombie->CanBeTargetedByProjectile(false)) return;
		const ColliderComponent* collider = zombie->GetColliderComponent();
		if (!collider || !collider->mEnabled || HorizontalOverlap(collider->GetBoundingBox(),
			attackLeft, kAttackRectWidth) <= 0.0f) {
			return;
		}

		hitAny = true;
		if (zombie->HandleCaltropHit(*this)) {
			// 车辆自己拥有特殊受扎语义；普通/鎏金冰车与投篮车均由虚入口保持各自契约。
			return;
		}
		zombie->TakeDamage(kCaltropDamage, DamageSource::PLANT);
	});

	if (hitAny) {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY1, 0.28f);
	}
}

void Caltrop::StartAttack()
{
	mAttackCooldown = kAttackCycleSeconds;
	const float speed = kAttackAnimationSpeed * GetAttackSpeedMultiplier();
	PlayTrackOnce("anim_attack", "anim_idle", speed, 0.1f);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.25f);
}

void Caltrop::SaveExtraData(nlohmann::json& j) const
{
	j["attackCooldown"] = mAttackCooldown;
}

void Caltrop::LoadExtraData(const nlohmann::json& j)
{
	mAttackCooldown = std::clamp(
		j.value("attackCooldown", 0.0f), 0.0f, kAttackCycleSeconds);
}

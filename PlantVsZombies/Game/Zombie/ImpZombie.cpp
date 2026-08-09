#include "ImpZombie.h"

#include "ImpCharred.h"
#include "../AudioSystem.h"
#include "../GameObjectManager.h"
#include "../ShadowComponent.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../GameRandom.h"
#include "../../ResourceKeys.h"
#include "../../ResourceManager.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr int kBodyHealth = 270;                         // 原版经典小鬼本体生命
	constexpr int kEatFrameOne = 44;                         // 主人指定的第一处啃食结算全局帧
	constexpr int kEatFrameTwo = 55;                         // 主人指定的第二处啃食结算全局帧
	constexpr int kDeathFrame = 81;                          // 主人指定的普通死亡回收全局帧
	constexpr float kThrownClipSpeed = 18.0f / 12.0f;        // 原版抛出循环 18fps 相对资源 12fps
	constexpr float kLandClipSpeed = 24.0f / 12.0f;          // 原版落地 24fps 相对资源 12fps
	constexpr float kEatClipSpeed = 24.0f / 12.0f;           // 小鬼啃食轨采用原版常用 24fps
	constexpr float kHorizontalThrowSpeed = 300.0f;          // 原版每厘秒 3px，折算 px/s
	constexpr float kThrowGravity = 500.0f;                  // 原版每厘秒 0.05px，折算 px/s^2
	constexpr float kInitialAltitude = 112.0f;               // 小鬼脱手高度；按主人目验在原版 88px 基础上上抬 24px
	constexpr float kLimbVolume = 0.25f;                     // 小鬼断肢断头音量
	constexpr float kColliderWidth = 40.0f;                  // 小鬼碰撞框宽度，单位 px
	constexpr float kColliderHeight = 70.0f;                 // 小鬼碰撞框高度，单位 px
	constexpr float kColliderOffsetX = -15.0f;                // 原版碰撞框左缘相对逻辑原点 X，单位 px
	constexpr float kColliderOffsetY = -20.0f;                // 原版碰撞框上缘相对逻辑原点 Y，单位 px
}

void ImpZombie::SetupZombie()
{
	mBodyHealth = kBodyHealth;
	mBodyMaxHealth = kBodyHealth;
	mHasTongue = false;
	mPhase = Phase::WALKING;
	mAltitude = 0.0f;

	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetScale(Vector(0.6f, 0.6f));
		shadow->SetOffset(Vector(16.0f, 38.0f));
	}

	if (mCollider) {
		mCollider->size = Vector(kColliderWidth, kColliderHeight);
		mCollider->offset = Vector(kColliderOffsetX, kColliderOffsetY);
	}

	RegisterFrameEvents();
	if (mIsPreview) {
		PlayTrack("anim_walk");
		return;
	}
	mSpeed += GameRandom::Range(-3, 3);
	PlayWalkAnimation(0.0f);
}

/** 注册小鬼专属的两次啃食命中与死亡回收帧。 */
void ImpZombie::RegisterFrameEvents()
{
	mAnimator->AddFrameEvent(kEatFrameOne, [this]() { EatTarget(); }, true);
	mAnimator->AddFrameEvent(kEatFrameTwo, [this]() { EatTarget(); }, true);
	mAnimator->AddFrameEvent(kDeathFrame, [this]() { Die(); });
}

void ImpZombie::ConfigureThrown(float throwDistance, bool movingRight,
	float inheritedCooldown, bool inheritMindControl)
{
	if (mIsDead || mIsDying || !mAnimator) return;
	// CanBeCharmed 仅允许地面态，所以必须先提交阵营，再切换为 THROWN。
	if (inheritMindControl) StartMindControlled();
	mPhase = Phase::THROWN;
	mAltitude = kInitialAltitude;
	mHorizontalVelocity = kHorizontalThrowSpeed;
	mThrowMovingRight = movingRight;
	const float flightSeedSeconds = std::max(0.0f, throwDistance) / kHorizontalThrowSpeed;
	mVerticalVelocity = 0.5f * flightSeedSeconds * kThrowGravity;
	PlayTrack("anim_thrown", kThrownClipSpeed);
	if (inheritedCooldown > 0.0f) SetCooldown(inheritedCooldown);
	ApplyPhasePresentation();
}

void ImpZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (mPhase == Phase::THROWN) {
		if (!transform || scaledDelta <= 0.0f) return;
		const float direction = mThrowMovingRight ? 1.0f : -1.0f;
		transform->Translate(direction * mHorizontalVelocity * scaledDelta, 0.0f);
		return;
	}
	if (mPhase == Phase::WALKING) {
		Zombie::ZombieMove(scaledDelta, transform);
	}
}

void ImpZombie::ZombieUpdate(float scaledTime)
{
	if (mPhase == Phase::THROWN) {
		mVerticalVelocity -= kThrowGravity * scaledTime;
		mAltitude += mVerticalVelocity * scaledTime;
		if (mAltitude <= 0.0f) BeginLanding();
		return;
	}
	if (mPhase == Phase::LANDING && mAnimator && !mAnimator->IsPlaying()) {
		FinishLanding();
	}
}

void ImpZombie::BeginLanding()
{
	if (mPhase != Phase::THROWN) return;
	mPhase = Phase::LANDING;
	mAltitude = 0.0f;
	mVerticalVelocity = 0.0f;
	PlayTrackOnce("anim_land", "", kLandClipSpeed, 0.0f);
	ApplyPhasePresentation();
}

void ImpZombie::FinishLanding()
{
	if (mPhase != Phase::LANDING) return;
	mPhase = Phase::WALKING;
	ApplyPhasePresentation();
	PlayWalkAnimation(0.1f);
}

void ImpZombie::PlayWalkAnimation(float blendTime)
{
	if (mPhase == Phase::WALKING) {
		PlayTrack("anim_walk", 0.0f, blendTime);
	}
}

void ImpZombie::StartEat(ColliderComponent* other)
{
	if (mPhase != Phase::WALKING) return;
	Zombie::StartEat(other);
}

void ImpZombie::OnStartEating()
{
	PlayTrack("anim_eat", kEatClipSpeed, 0.2f);
}

bool ImpZombie::CanBeTargetedByProjectile(bool targetsFlying) const
{
	return mPhase == Phase::WALKING && !targetsFlying;
}

bool ImpZombie::CanBeChilled() const
{
	return mPhase == Phase::WALKING && Zombie::CanBeChilled();
}

Vector ImpZombie::GetVisualPosition() const
{
	return Zombie::GetVisualPosition() + Vector(0.0f, -mAltitude);
}

void ImpZombie::ArmDrop()
{
	if (!mHasArm || !mAnimator) return;
	const Vector particlePosition = GetArmParticleAnchor();
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_imp_outerarm_upper",
		ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_REANIM_ZOMBIE_IMP_ARM1_BONE));
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ImpZombieArmOff", particlePosition);
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, kLimbVolume);
}

void ImpZombie::HeadDrop()
{
	if (!mHasHead || !mAnimator) return;
	const Vector particlePosition = GetHeadParticleAnchor();
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ImpZombieHeadOff", particlePosition);
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, kLimbVolume);
}

void ImpZombie::ApplyPhasePresentation() const
{
	if (mCollider) {
		mCollider->mEnabled = mPhase == Phase::WALKING && !mIsDying && !mIsDead;
	}
	if (mPoolShadow) {
		mPoolShadow->mEnabled = mPhase != Phase::THROWN && !mInPool;
	}
}

void ImpZombie::ZombieItemUpdate() const
{
	Zombie::ZombieItemUpdate();
	if (!mHasArm && mAnimator) {
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackImage("Zombie_imp_outerarm_upper",
			ResourceManager::GetInstance().GetTexture(
				ResourceKeys::Textures::IMAGE_REANIM_ZOMBIE_IMP_ARM1_BONE));
	}
	if (!mHasHead && mAnimator) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
	}
	ApplyPhasePresentation();
}

void ImpZombie::Charred()
{
	if (mIsDead || !mBoard) return;
	GameObjectManager::GetInstance().CreateGameObjectImmediate<ImpCharred>(
		LAYER_GAME_ZOMBIE, mBoard, GetVisualPosition());
	Die();
}

void ImpZombie::SaveExtraData(nlohmann::json& j) const
{
	j["phase"] = static_cast<int>(mPhase);
	j["altitude"] = mAltitude;
	j["verticalVelocity"] = mVerticalVelocity;
	j["horizontalVelocity"] = mHorizontalVelocity;
	j["throwMovingRight"] = mThrowMovingRight;
}

void ImpZombie::LoadExtraData(const nlohmann::json& j)
{
	mPhase = static_cast<Phase>(std::clamp(j.value("phase", 0), 0,
		static_cast<int>(Phase::LANDING)));
	mAltitude = std::clamp(j.value("altitude", 0.0f), 0.0f, 1000.0f);
	mVerticalVelocity = std::clamp(j.value("verticalVelocity", 0.0f),
		-2000.0f, 2000.0f);
	mHorizontalVelocity = std::clamp(j.value("horizontalVelocity",
		kHorizontalThrowSpeed), 0.0f, 1000.0f);
	mThrowMovingRight = j.value("throwMovingRight", mIsMindControlled);

	if (mIsDying || mIsDead) {
		mPhase = Phase::WALKING;
		mAltitude = 0.0f;
		mVerticalVelocity = 0.0f;
	}
	else if (mPhase == Phase::THROWN && GetCurrentTrackName() != "anim_thrown") {
		PlayTrack("anim_thrown", kThrownClipSpeed);
	}
	else if (mPhase == Phase::LANDING && GetCurrentTrackName() != "anim_land") {
		PlayTrackOnce("anim_land", "", kLandClipSpeed, 0.0f);
	}
	else if (mPhase == Phase::WALKING
		&& (GetCurrentTrackName() == "anim_thrown" || GetCurrentTrackName() == "anim_land")) {
		PlayWalkAnimation(0.0f);
	}
	ApplyPhasePresentation();
}

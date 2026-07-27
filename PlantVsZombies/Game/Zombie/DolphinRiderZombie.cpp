#include "DolphinRiderZombie.h"

#include "../Board.h"
#include "../Plant/Plant.h"
#include "../ShadowComponent.h"
#include "../../ParticleSystem/ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
	constexpr float kGroundRootMotionRate = 12.0f;   // 根轨每帧位移换算为每秒位移的资源帧率，禁止拿它直接放大移速
	constexpr float kFastGroundClipSpeed = 2.25f;    // 原版 0.9 px/tick、47 tick/s 对齐为约 27 FPS 和 42 px/s
	constexpr float kSlowGroundClipSpeed = 0.75f;    // 原版弃豚平均 0.3 px/tick 对齐为约 9 FPS 和 14 px/s
	constexpr float kRideWorldSpeed = 54.0f;         // anim_ride 没有根运动，按原版 0.9 px/tick 手动推进
	constexpr float kJumpWorldSpeed = 30.0f;         // 原版跳跃前半段 mVelX=0.5 px/tick
	constexpr float kEntryClipSpeed = 16.0f / 12.0f; // 原版入水 16 FPS 相对资源 12 FPS 的倍率
	constexpr float kJumpClipSpeed = 10.0f / 12.0f;  // 原版跃豚 10 FPS 相对资源 12 FPS 的倍率
	constexpr float kEntrySplashProgress = 0.56f;    // C# 入水水花节点
	constexpr float kJumpBlockProgress = 0.30f;      // C# 高坚果阻拦检查节点
	constexpr float kJumpSplashProgress = 0.49f;     // C# 海豚重新入水节点
	constexpr float kEntryWorldShift = 70.0f;        // 入水换轨时提交的相对根位移，单位 px
	constexpr float kJumpWorldShift = 94.0f;         // 正常越过植物后提交的相对根位移，单位 px
	constexpr float kRideVisualLift = 45.0f;         // 骑乘资源基准较低，抬升到与弃豚游泳一致的水面线
	constexpr float kJumpLandingExtraLift = 15.0f;   // 跳跃末帧与 anim_swim 首帧的剩余垂直基准差
	constexpr float kJumpAltitude = 10.0f;           // 跳跃中用于平滑轨道衔接的最大视觉高度，单位 px
	constexpr float kBlockedLandingGap = 5.0f;       // 阻拦后碰撞箱与植物右边缘保留的间距，单位 px
	constexpr float kPi = 3.14159265f;               // 跳跃视觉高度正弦插值使用的圆周率

	float SmoothStep(float value)
	{
		const float t = std::clamp(value, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}
}

bool DolphinRiderZombie::HasDolphin() const
{
	return mPhase == Phase::APPROACHING
		|| mPhase == Phase::ENTERING_POOL
		|| mPhase == Phase::RIDING
		|| mPhase == Phase::JUMPING;
}

float DolphinRiderZombie::GetJumpProgress() const
{
	if (mPhase != Phase::JUMPING || !mAnimator) return 0.0f;
	const auto [begin, end] = mAnimator->GetTrackRange("anim_dolphinjump");
	if (end <= begin) return 0.0f;
	return std::clamp((GetCurrentFrame() - static_cast<float>(begin))
		/ static_cast<float>(end - begin), 0.0f, 1.0f);
}

Vector DolphinRiderZombie::GetDolphinVisualCompensation() const
{
	if (mPhase == Phase::ENTERING_POOL && mAnimator) {
		const auto [begin, end] = mAnimator->GetTrackRange("anim_jumpinpool");
		const float progress = end > begin
			? std::clamp((GetCurrentFrame() - static_cast<float>(begin))
				/ static_cast<float>(end - begin), 0.0f, 1.0f)
			: 0.0f;
		const float liftProgress = SmoothStep(
			(progress - kEntrySplashProgress) / (1.0f - kEntrySplashProgress));
		return Vector(0.0f, -kRideVisualLift * liftProgress);
	}
	if (mPhase == Phase::RIDING) return Vector(kEntryWorldShift, -kRideVisualLift);
	if (mPhase != Phase::JUMPING) return Vector::zero();

	const float progress = GetJumpProgress();
	const float altitude = kJumpAltitude * std::sin(progress * kPi);
	const float landingProgress = SmoothStep(
		(progress - kJumpSplashProgress) / (1.0f - kJumpSplashProgress));
	return Vector(kEntryWorldShift + altitude,
		-kRideVisualLift - altitude - kJumpLandingExtraLift * landingProgress);
}

void DolphinRiderZombie::SetupZombie()
{
	mBodyMaxHealth = 500;
	mBodyHealth = 500;
	mSpeed = kGroundRootMotionRate;
	mNeedDropArm = false;
	SetAnimationSpeed(1.0f);

	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetOffset(Vector(8.0f, 42.0f));
	}
	if (mIsPreview) {
		PlayTrack("anim_idle");
		return;
	}

	RegisterFrameEvents();
	if (mCollider) {
		mBaseColliderOffsetX = mCollider->offset.x;
	}
	PlayTrack("anim_walkdolphin", kFastGroundClipSpeed);
	ApplyPhasePresentation();
}

/** 注册主人按当前资源确认的两次啃食命中与死亡终点。 */
void DolphinRiderZombie::RegisterFrameEvents()
{
	mAnimator->AddFrameEvent(182, [this]() { EatTarget(); }, true);
	mAnimator->AddFrameEvent(201, [this]() { EatTarget(); }, true);
	mAnimator->AddFrameEvent(288, [this]() { Die(); });
}

void DolphinRiderZombie::PlaySpawnSound()
{
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_DOLPHIN_APPEARS, 0.45f);
}

void DolphinRiderZombie::StartEat(ColliderComponent* other)
{
	if (!other || mIsDying) return;
	auto* plant = dynamic_cast<Plant*>(other->GetGameObject());
	if (mPhase == Phase::RIDING) {
		if (plant && plant->mRow == mRow && plant->CanBeEaten()) {
			BeginJump(plant);
		}
		return;
	}
	if (mPhase != Phase::SWIMMING && mPhase != Phase::WALKING_WITHOUT_DOLPHIN) return;
	Zombie::StartEat(other);
}

void DolphinRiderZombie::BeginEnteringPool()
{
	if (mPhase != Phase::APPROACHING) return;
	mPhase = Phase::ENTERING_POOL;
	mEntrySplashPlayed = false;
	PlayTrackOnce("anim_jumpinpool", "anim_ride", kEntryClipSpeed, 0.1f, 1.0f);
	ApplyPhasePresentation();
}

void DolphinRiderZombie::FinishEnteringPool()
{
	if (mPhase != Phase::ENTERING_POOL) return;
	if (auto* transform = GetTransformComponent()) {
		transform->Translate(mIsMindControlled ? kEntryWorldShift : -kEntryWorldShift, 0.0f);
	}
	mPhase = Phase::RIDING;
	mSpeed = kGroundRootMotionRate;
	ApplyPhasePresentation();
}

void DolphinRiderZombie::BeginJump(Plant* target)
{
	if (mPhase != Phase::RIDING || !target) return;
	mPhase = Phase::JUMPING;
	mJumpTargetPlantID = target->mPlantID;
	mJumpSplashPlayed = false;
	mJumpBlockChecked = false;
	PlayTrackOnce("anim_dolphinjump", "anim_swim", kJumpClipSpeed, 0.1f, 1.0f);
	AudioSystem::PlaySound(
		ResourceKeys::Sounds::SOUND_DOLPHIN_BEFORE_JUMPING, 0.45f);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PLANT_WATER, 0.35f);
	ApplyPhasePresentation();
}

void DolphinRiderZombie::FinishJump(bool blocked)
{
	if (mPhase != Phase::JUMPING) return;

	if (blocked && mBoard) {
		if (auto* plant = mBoard->mEntityManager.GetPlant(mJumpTargetPlantID)) {
			if (const auto* plantCollider = plant->GetColliderComponent()) {
				const SDL_FRect bounds = plantCollider->GetBoundingBox();
				Vector position = GetPosition();
				position.x = bounds.x + bounds.w + kBlockedLandingGap - mBaseColliderOffsetX;
				SetPosition(position);
			}
		}
	}
	else if (auto* transform = GetTransformComponent()) {
		transform->Translate(mIsMindControlled ? kJumpWorldShift : -kJumpWorldShift, 0.0f);
	}

	mPhase = mInPool ? Phase::SWIMMING : Phase::WALKING_WITHOUT_DOLPHIN;
	mJumpTargetPlantID = NULL_PLANT_ID;
	mSpeed = kGroundRootMotionRate;
	mNeedDropArm = true;
	PlayWalkAnimation(0.0f);
	ApplyPhasePresentation();
	CheckDeferredArmDrop();
}

void DolphinRiderZombie::ZombieUpdate(float)
{
	if (mPhase == Phase::ENTERING_POOL) {
		const auto [begin, end] = mAnimator->GetTrackRange("anim_jumpinpool");
		const float progress = end > begin
			? std::clamp((GetCurrentFrame() - static_cast<float>(begin))
				/ static_cast<float>(end - begin), 0.0f, 1.0f)
			: 0.0f;
		if (!mEntrySplashPlayed && progress >= kEntrySplashProgress) {
			mEntrySplashPlayed = true;
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_ENTERING_WATER, 0.4f);
		}
		if (GetCurrentTrackName() == "anim_ride") FinishEnteringPool();
		return;
	}

	if (mPhase != Phase::JUMPING) return;
	const float progress = GetJumpProgress();
	if (!mJumpBlockChecked && progress >= kJumpBlockProgress) {
		mJumpBlockChecked = true;
		if (mBoard) {
			if (auto* plant = mBoard->mEntityManager.GetPlant(mJumpTargetPlantID);
				plant && plant->BlocksZombieJump(ZombieJumpType::DOLPHIN_RIDER)) {
				plant->OnZombieJumpBlocked(ZombieJumpType::DOLPHIN_RIDER);
				FinishJump(true);
				return;
			}
		}
	}
	if (!mJumpSplashPlayed && progress >= kJumpSplashProgress) {
		mJumpSplashPlayed = true;
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_ENTERING_WATER, 0.4f);
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PLANT_WATER, 0.35f);
	}
	if (GetCurrentTrackName() == "anim_swim") FinishJump(false);
}

void DolphinRiderZombie::MoveManually(
	float speed, float scaledDelta, TransformComponent* transform)
{
	if (!transform || !mAnimator) return;
	float multiplier = mAnimator->GetExtraSpeedMultiplier();
	if (mBoard) {
		multiplier *= AmplifySpeedMultiplierForGoldenIce(
			mBoard->GetZombieWindMoveMultiplier(mIsMindControlled));
	}
	const float distance = speed * multiplier * scaledDelta;
	transform->Translate(mIsMindControlled ? distance : -distance, 0.0f);
}

void DolphinRiderZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (mPhase == Phase::ENTERING_POOL) return;
	if (mPhase == Phase::RIDING) {
		MoveManually(kRideWorldSpeed, scaledDelta, transform);
		return;
	}
	if (mPhase == Phase::JUMPING) {
		if (GetJumpProgress() < kJumpSplashProgress) {
			MoveManually(kJumpWorldSpeed, scaledDelta, transform);
		}
		return;
	}
	Zombie::ZombieMove(scaledDelta, transform);
}

void DolphinRiderZombie::PlayWalkAnimation(float blendTime)
{
	if (mPhase == Phase::APPROACHING) {
		if (mInPool) BeginEnteringPool();
		else PlayTrack("anim_walkdolphin", kFastGroundClipSpeed, blendTime);
		return;
	}
	if (mPhase == Phase::ENTERING_POOL || mPhase == Phase::JUMPING) return;
	if (mPhase == Phase::RIDING) {
		if (!mInPool) {
			// 未遇到植物便穿过整段泳池时，按 C# 回到带豚陆地步行，而不是让 anim_ride 滑上岸。
			mPhase = Phase::APPROACHING;
			mSpeed = kGroundRootMotionRate;
			PlayTrack("anim_walkdolphin", kFastGroundClipSpeed, blendTime);
			ApplyPhasePresentation();
			return;
		}
		PlayTrack("anim_ride", 1.0f, blendTime);
		return;
	}
	if (mInPool) {
		mPhase = Phase::SWIMMING;
		PlayTrack("anim_swim", kSlowGroundClipSpeed, blendTime);
	}
	else {
		mPhase = Phase::WALKING_WITHOUT_DOLPHIN;
		PlayTrack("anim_walk", kSlowGroundClipSpeed, blendTime);
	}
}

void DolphinRiderZombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	if (!mInPool) {
		// 粒子配方本身包含头部偏移；从逻辑原点发射，避免再次叠加该品种的视觉偏移。
		g_particleSystem->EmitEffect("ZombieDolphinRiderHeadOff", GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LIMBS_POP, 0.35f);
}

void DolphinRiderZombie::ArmDrop()
{
	if (!mHasArm) return;
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_dolphinrider_outerarm_upper",
		ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_ZOMBIE_DOLPHINRIDER_OUTERARM_UPPER2));
	if (!mInPool) {
		g_particleSystem->EmitEffect("ZombieArmOff", GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LIMBS_POP, 0.35f);
}

void DolphinRiderZombie::ZombieItemUpdate() const
{
	Zombie::ZombieItemUpdate();
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
	}
	if (!mHasArm) {
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackImage("Zombie_dolphinrider_outerarm_upper",
			ResourceManager::GetInstance().GetTexture(
				ResourceKeys::Textures::IMAGE_ZOMBIE_DOLPHINRIDER_OUTERARM_UPPER2));
	}
	ApplyPhasePresentation();
}

Vector DolphinRiderZombie::GetVisualPosition() const
{
	return Zombie::GetVisualPosition() + GetDolphinVisualCompensation();
}

bool DolphinRiderZombie::CanBeCharmed() const
{
	return mPhase == Phase::SWIMMING || mPhase == Phase::WALKING_WITHOUT_DOLPHIN;
}

bool DolphinRiderZombie::CanBeFrozen() const
{
	return mPhase != Phase::ENTERING_POOL && mPhase != Phase::JUMPING;
}

bool DolphinRiderZombie::CanBeGrabbedByTangleKelp() const
{
	return mPhase == Phase::SWIMMING || mPhase == Phase::WALKING_WITHOUT_DOLPHIN;
}

bool DolphinRiderZombie::ShouldPlayDeathAnimation() const
{
	return mPhase == Phase::SWIMMING || mPhase == Phase::WALKING_WITHOUT_DOLPHIN;
}

void DolphinRiderZombie::CheckDeferredArmDrop()
{
	if (!mHasArm || mBodyHealth > static_cast<int64_t>(mBodyMaxHealth) * 2 / 3) return;
	ArmDrop();
	mHasArm = false;
}

void DolphinRiderZombie::ApplyPhasePresentation() const
{
	if (mCollider && !mIsPreview) {
		// reanim 的骑乘身体轨道自身已向左偏约 70 px；碰撞箱保持逻辑原点即可对齐可见身体。
		mCollider->offset.x = mBaseColliderOffsetX;
		mCollider->mEnabled = !mIsDying
			&& mPhase != Phase::ENTERING_POOL
			&& mPhase != Phase::JUMPING;
	}
	if (mPoolShadow) {
		mPoolShadow->mEnabled = mPhase != Phase::ENTERING_POOL && mPhase != Phase::JUMPING;
	}
}

void DolphinRiderZombie::SaveExtraData(nlohmann::json& j) const
{
	j["phase"] = static_cast<int>(mPhase);
	j["jumpTargetPlantID"] = mJumpTargetPlantID;
	j["entrySplashPlayed"] = mEntrySplashPlayed;
	j["jumpSplashPlayed"] = mJumpSplashPlayed;
	j["jumpBlockChecked"] = mJumpBlockChecked;
}

void DolphinRiderZombie::LoadExtraData(const nlohmann::json& j)
{
	const int phase = std::clamp(j.value("phase", 0), 0,
		static_cast<int>(Phase::WALKING_WITHOUT_DOLPHIN));
	mPhase = static_cast<Phase>(phase);
	mJumpTargetPlantID = j.value("jumpTargetPlantID", NULL_PLANT_ID);
	mEntrySplashPlayed = j.value("entrySplashPlayed", false);
	mJumpSplashPlayed = j.value("jumpSplashPlayed", false);
	mJumpBlockChecked = j.value("jumpBlockChecked", false);
	mNeedDropArm = !HasDolphin();
	mSpeed = kGroundRootMotionRate;
	ApplyPhasePresentation();
}

#include "GatlingPea.h"

#include "../Board.h"
#include "../../GameApp.h"

namespace {
	constexpr float kGatlingShootingClipSpeed = 1.9f; // 保证第 80 帧在约 1.5 秒攻击周期内完成
	constexpr float kBodyIdleBasePoseX = 37.6f;       // anim_idle 首帧锚点 X，用于消除附件重复位移
	constexpr float kBodyIdleBasePoseY = 48.7f;       // anim_idle 首帧锚点 Y，用于消除附件重复位移
}

void GatlingPea::SetupPlant()
{
	Plant::SetupPlant();
	const auto reanim = mAnimator ? mAnimator->GetReanimation() : nullptr;
	if (!reanim) return;

	// 根 Animator 只播 0..24 的身体；独立头 Animator 播 25..89 的待机/射击分件。
	// 这份时间线没有 anim_stem，因此挂在 anim_idle；附件接口只乘 current，
	// 需要预先减去首帧基准位姿，避免头部再次叠加身体锚点而整体偏移。
	mAnimator->PlayTrack("anim_idle");
	mAnimator->SetSpeed(GameRandom::Range(1.1f, 1.3f));
	mHeadAnim = std::make_shared<Animator>(reanim);
	mHeadAnim->SetSpeed(mAnimator->GetSpeed());
	mHeadAnim->PlayTrack("anim_head_idle");
	mHeadAnim->SetLocalPosition(-kBodyIdleBasePoseX, -kBodyIdleBasePoseY);
	if (!mAnimator->AttachAnimator("anim_idle", mHeadAnim)) {
		mHeadAnim.reset();
		return;
	}

	// 主人提供的帧号已是 AddFrameEvent 口径，不再额外减一。
	for (const int frame : {60, 68, 74, 80}) {
		mHeadAnim->AddFrameEvent(frame, [this]() {
			PlayShotSound();
			ShootBullet();
		}, true);
	}
}

void GatlingPea::PlantUpdate()
{
	const float multiplier = GetAttackSpeedMultiplier();
	mShootTimer += DeltaTime::GetDeltaTime() * multiplier;
	if (mShootTimer < mShootTime || !HasZombieInRow()) return;

	mShootTimer = 0.0f;
	mHeadAnim->PlayTrackOnce(
		"anim_shooting", "anim_head_idle",
		kGatlingShootingClipSpeed * multiplier, 0.1f);
}

void GatlingPea::ShootBullet()
{
	if (!mBoard) return;
	const Vector bulletPosition = GetPosition() + Vector(30.0f, -30.0f);
	mBoard->CreatePlantBullet(
		BulletType::BULLET_PEA, mRow, bulletPosition, mPlantType);
}

void GatlingPea::PlayShotSound() const
{
	AudioSystem::PlaySound(GameRandom::Chance()
		? ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT
		: ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
}

#include "SplitPea.h"

#include "Game/Board/Board.h"
#include "../Bullet/Bullet.h"
#include "../Zombie/Zombie.h"

namespace {
	constexpr int kForwardFireFrame = 95;              // 主人确认的前头真实发射帧
	constexpr int kRearFireFrame = 57;                 // 主人确认的后头真实发射帧
	constexpr float kReanimFramesPerSecond = 12.0f;    // SplitPea.reanim 的基础帧率
	constexpr float kForwardShootFramesPerSecond = 45.0f; // C# 前头射击动画帧率
	constexpr float kRearShootFramesPerSecond = 35.0f; // C# 后头射击动画帧率
	constexpr float kIdleFramesPerSecondMin = 15.0f;   // C# 待机随机帧率下限
	constexpr float kIdleFramesPerSecondMax = 20.0f;   // C# 待机随机帧率上限
	constexpr float kShootBlendSeconds = 0.2f;         // C# StartBlend(20) 的进入/返回时长
	constexpr float kAttachmentBasePoseX = 37.6f;      // 根 anim_idle 首帧附件基准 X
	constexpr float kAttachmentBasePoseY = 48.7f;      // 根 anim_idle 首帧附件基准 Y
	constexpr float kForwardBulletOffsetX = 30.0f;     // 沿用项目普通射手的稳定前枪口 X
	constexpr float kRearBulletOffsetX = -58.0f;       // 原版后枪口比前枪口向左 88px
	constexpr float kBulletOffsetY = -30.0f;           // 两个头共用的稳定枪口 Y
	constexpr float kRearBulletVelocityX = -290.0f;    // 后向豌豆水平速度，单位 px/s
	constexpr float kRearTargetMaxOffsetX = -24.0f;    // C# 后向攻击矩形最右缘相对格心 X
}

void SplitPea::SetupPlant()
{
	Plant::SetupPlant();
	if (!mAnimator || !mAnimator->GetReanimation()) return;

	const float idleSpeed = GameRandom::Range(
		kIdleFramesPerSecondMin / kReanimFramesPerSecond,
		kIdleFramesPerSecondMax / kReanimFramesPerSecond);
	mAnimator->PlayTrack("anim_idle");
	mAnimator->SetSpeed(idleSpeed);

	// C# 将两个全尺寸子 reanim 都附着到根 anim_idle；当前附件实现不自动乘
	// inverse(basePose)，因此两个头都先抵消根轨首帧的基准位姿。
	mHeadAnim = CreateHeadAnimator("anim_head_idle");
	mRearHeadAnim = CreateHeadAnimator("anim_splitpea_idle");

	if (mHeadAnim) {
		mHeadAnim->AddFrameEvent(kForwardFireFrame, [this]() {
			PlayShootSound();
			ShootBullet();
		}, true);
	}
	if (mRearHeadAnim) {
		mRearHeadAnim->AddFrameEvent(kRearFireFrame, [this]() {
			PlayShootSound();
			ShootRearBullet();
		}, true);
	}
}

std::shared_ptr<Animator> SplitPea::CreateHeadAnimator(const char* idleTrack)
{
	auto reanim = mAnimator ? mAnimator->GetReanimation() : nullptr;
	if (!reanim) return nullptr;

	auto head = std::make_shared<Animator>(reanim);
	head->SetSpeed(mAnimator->GetSpeed());
	head->PlayTrack(idleTrack);
	head->SetLocalPosition(-kAttachmentBasePoseX, -kAttachmentBasePoseY);
	if (!mAnimator->AttachAnimator("anim_idle", head)) return nullptr;
	return head;
}

void SplitPea::PlantUpdate()
{
	const float attackSpeed = GetAttackSpeedMultiplier();

	// 第一颗后向豌豆的帧事件在同一逻辑步置位；紧接着重播后头射击轨，构成原版双发。
	if (mRearSecondShotPending && mRearHeadAnim) {
		mRearSecondShotPending = false;
		StartRearShot(attackSpeed);
	}

	mShootTimer += DeltaTime::GetDeltaTime() * attackSpeed;
	if (mShootTimer < mShootTime) return;

	// 原版 LaunchCounter 到期后无论是否找到目标都会开始下一轮计时。
	mShootTimer = 0.0f;
	if (HasTargetInDirection(true)) {
		StartForwardShot(attackSpeed);
	}
	if (!mRearSecondShotInBurst && HasTargetInDirection(false)) {
		StartRearShot(attackSpeed);
	}
}

bool SplitPea::HasTargetInDirection(bool forward) const
{
	if (!mBoard) return false;

	const float plantX = GetPosition().x;
	bool found = false;
	mBoard->mEntityRegistry.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (found || !zombie || zombie->IsMindControlled() || !zombie->HasHead()) return;

		const float zombieX = zombie->GetPosition().x;
		const bool isInDirection = forward
			? zombieX >= plantX
			: zombieX <= plantX + kRearTargetMaxOffsetX;
		if (isInDirection && mBoard->CanPlantAcquireZombie(this, zombie)) {
			found = true;
		}
	});
	return found;
}

void SplitPea::StartForwardShot(float attackSpeedMultiplier)
{
	if (!mHeadAnim) return;
	const float shootSpeed =
		(kForwardShootFramesPerSecond / kReanimFramesPerSecond)
		* attackSpeedMultiplier;
	mHeadAnim->PlayTrackOnce(
		"anim_shooting", "anim_head_idle", shootSpeed,
		kShootBlendSeconds, 0.0f, kShootBlendSeconds);
}

void SplitPea::StartRearShot(float attackSpeedMultiplier)
{
	if (!mRearHeadAnim) return;
	const float shootSpeed =
		(kRearShootFramesPerSecond / kReanimFramesPerSecond)
		* attackSpeedMultiplier;
	mRearHeadAnim->PlayTrackOnce(
		"anim_splitpea_shooting", "anim_splitpea_idle", shootSpeed,
		kShootBlendSeconds, 0.0f, kShootBlendSeconds);
}

void SplitPea::ShootBullet()
{
	if (!mBoard) return;
	const Vector bulletPosition =
		GetPosition() + Vector(kForwardBulletOffsetX, kBulletOffsetY);
	mBoard->CreatePlantBullet(BulletType::BULLET_PEA, mRow, bulletPosition, mPlantType);
}

void SplitPea::ShootRearBullet()
{
	if (!mBoard) return;
	const Vector bulletPosition =
		GetPosition() + Vector(kRearBulletOffsetX, kBulletOffsetY);
	Bullet* bullet = mBoard->CreatePlantBullet(
		BulletType::BULLET_PEA, mRow, bulletPosition, mPlantType);
	if (!bullet) return;
	bullet->SetVelocityX(kRearBulletVelocityX);

	if (!mRearSecondShotInBurst) {
		mRearSecondShotPending = true;
		mRearSecondShotInBurst = true;
	}
	else {
		mRearSecondShotInBurst = false;
	}
}

void SplitPea::PlayShootSound() const
{
	const std::string& sound = GameRandom::Chance()
		? ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT
		: ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2;
	AudioSystem::PlaySound(sound, 0.3f);
}

void SplitPea::SaveExtraData(nlohmann::json& j) const
{
	Shooter::SaveExtraData(j);
	SaveHeadAnimatorState(j, "rearHeadAnim", mRearHeadAnim.get());
	j["rearSecondShotPending"] = mRearSecondShotPending;
	j["rearSecondShotInBurst"] = mRearSecondShotInBurst;
}

void SplitPea::LoadExtraData(const nlohmann::json& j)
{
	Shooter::LoadExtraData(j);
	LoadHeadAnimatorState(j, "rearHeadAnim", mRearHeadAnim.get());
	mRearSecondShotPending = j.value("rearSecondShotPending", false);
	mRearSecondShotInBurst = j.value("rearSecondShotInBurst", false);
}

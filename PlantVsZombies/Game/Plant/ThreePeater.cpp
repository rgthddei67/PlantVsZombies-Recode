#include "ThreePeater.h"

#include "../Board.h"
#include "../Bullet/Bullet.h"
#include "../Zombie/Zombie.h"

#include <string>

namespace {
	constexpr int kHead1FireFrame = 29;              // 下路头真实发射帧；主人已按 AddFrameEvent 口径确认
	constexpr int kHead3FireFrame = 73;              // 上路头真实发射帧；主人已按 AddFrameEvent 口径确认
	constexpr int kHead2FireFrame = 111;             // 中路头真实发射帧；主人已按 AddFrameEvent 口径确认
	constexpr float kReanimFramesPerSecond = 12.0f;  // ThreePeater.reanim 的基础帧率
	constexpr float kShootFramesPerSecond = 20.0f;   // C# 三个头进入射击态时使用的动画帧率
	constexpr float kIdleFramesPerSecondMin = 15.0f; // C# 三线射手待机随机帧率下限
	constexpr float kIdleFramesPerSecondMax = 20.0f; // C# 三线射手待机随机帧率上限
	constexpr float kShootBlendSeconds = 0.1f;       // C# StartBlend(10) 对应的过渡时长
	constexpr float kBulletOffsetX = 5.0f;           // C# mX+45 换算到本项目格子中心后的水平偏移
	constexpr float kBulletOffsetY = -40.0f;         // C# mY+10 换算到本项目格子中心后的垂直偏移
	constexpr float kBoundaryCompensationBulletSpeed = 360.0f; // 边路补偿弹水平速度，和本行普通弹的 290px/s 拉开
	constexpr float kHead1BasePoseX = 53.0f;         // 根 anim_head1 在 anim_idle 首帧的基准锚点 X
	constexpr float kHead1BasePoseY = 50.8f;         // 根 anim_head1 在 anim_idle 首帧的基准锚点 Y
	constexpr float kHead2BasePoseX = 35.4f;         // 根 anim_head2 在 anim_idle 首帧的基准锚点 X
	constexpr float kHead2BasePoseY = 35.0f;         // 根 anim_head2 在 anim_idle 首帧的基准锚点 Y
	constexpr float kHead3BasePoseX = 21.0f;         // 根 anim_head3 在 anim_idle 首帧的基准锚点 X
	constexpr float kHead3BasePoseY = 22.2f;         // 根 anim_head3 在 anim_idle 首帧的基准锚点 Y

	std::string HeadStateKey(const char* prefix, const char* suffix)
	{
		return std::string(prefix) + suffix;
	}

	// 额外两个头不属于 AnimatedObject 根 Animator，必须完整保存一次性播放状态机。
	void SaveHeadAnimatorState(nlohmann::json& j, const char* prefix, const Animator* animator)
	{
		if (!animator) return;
		j[HeadStateKey(prefix, "Track")] = animator->GetCurrentTrackName();
		j[HeadStateKey(prefix, "Frame")] = animator->GetCurrentFrame();
		j[HeadStateKey(prefix, "Speed")] = animator->GetSpeed();
		j[HeadStateKey(prefix, "ClipSpeed")] = animator->GetClipSpeed();
		j[HeadStateKey(prefix, "PlayState")] = static_cast<int>(animator->GetPlayingState());
		j[HeadStateKey(prefix, "TargetTrack")] = animator->GetTargetTrack();
		j[HeadStateKey(prefix, "TargetTrackSpeed")] = animator->GetTargetTrackSpeed();
		j[HeadStateKey(prefix, "Playing")] = animator->IsPlaying();
	}

	// 恢复完整 PlayTrackOnce 状态；字段缺失时保留 SetupPlant 建立的待机状态。
	void LoadHeadAnimatorState(const nlohmann::json& j, const char* prefix, Animator* animator)
	{
		if (!animator) return;
		const std::string trackKey = HeadStateKey(prefix, "Track");
		if (!j.contains(trackKey)) return;

		const std::string track = j.value(trackKey, std::string{});
		if (track.empty()) return;

		const float clipSpeed = j.value(HeadStateKey(prefix, "ClipSpeed"), 0.0f);
		const int rawState = j.value(
			HeadStateKey(prefix, "PlayState"), static_cast<int>(PlayState::PLAY_REPEAT));
		const PlayState state = (rawState >= static_cast<int>(PlayState::PLAY_NONE)
			&& rawState <= static_cast<int>(PlayState::PLAY_ONCE_TO))
			? static_cast<PlayState>(rawState) : PlayState::PLAY_REPEAT;

		if (state == PlayState::PLAY_ONCE || state == PlayState::PLAY_ONCE_TO) {
			animator->PlayTrackOnce(
				track,
				j.value(HeadStateKey(prefix, "TargetTrack"), std::string{}),
				clipSpeed,
				0.0f,
				j.value(HeadStateKey(prefix, "TargetTrackSpeed"), 0.0f));
		}
		else {
			animator->PlayTrack(track, clipSpeed);
		}

		const std::string speedKey = HeadStateKey(prefix, "Speed");
		if (j.contains(speedKey)) {
			animator->SetSpeed(j.value(speedKey, 1.0f));
		}
		animator->SetCurrentFrame(j.value(HeadStateKey(prefix, "Frame"), 0.0f));
		if (j.contains(HeadStateKey(prefix, "Playing"))
			&& !j.value(HeadStateKey(prefix, "Playing"), true)) {
			animator->Pause();
		}
	}
}

void ThreePeater::SetupPlant()
{
	Plant::SetupPlant();

	auto reanim = mAnimator ? mAnimator->GetReanimation() : nullptr;
	if (!reanim) return;

	const float idleSpeed = GameRandom::Range(
		kIdleFramesPerSecondMin / kReanimFramesPerSecond,
		kIdleFramesPerSecondMax / kReanimFramesPerSecond);
	mAnimator->PlayTrack("anim_idle");
	mAnimator->SetSpeed(idleSpeed);

	// C# 将三个头分别附着在根动画的三个头部锚点，身体与每个头可独立循环。
	mHeadAnim = CreateHeadAnimator(
		"anim_head_idle1", "anim_head1",
		kHead1BasePoseX, kHead1BasePoseY, kHead1FireFrame, 1);
	mHeadAnim2 = CreateHeadAnimator(
		"anim_head_idle2", "anim_head2",
		kHead2BasePoseX, kHead2BasePoseY, kHead2FireFrame, 0);
	mHeadAnim3 = CreateHeadAnimator(
		"anim_head_idle3", "anim_head3",
		kHead3BasePoseX, kHead3BasePoseY, kHead3FireFrame, -1);
}

std::shared_ptr<Animator> ThreePeater::CreateHeadAnimator(
	const char* idleTrack, const char* attachTrack,
	float basePoseX, float basePoseY, int fireFrame, int targetRowOffset)
{
	auto reanim = mAnimator ? mAnimator->GetReanimation() : nullptr;
	if (!reanim) return nullptr;

	auto head = std::make_shared<Animator>(reanim);
	head->SetSpeed(mAnimator->GetSpeed());
	head->PlayTrack(idleTrack);
	// C# GetAttachmentOverlayMatrix 使用 current * inverse(basePose)；当前附件接口只乘
	// current，因此在局部坐标先减去 anim_idle 首帧锚点，精确复原三个头各自的基准姿态。
	head->SetLocalPosition(-basePoseX, -basePoseY);
	if (!mAnimator->AttachAnimator(attachTrack, head)) return nullptr;

	head->AddFrameEvent(fireFrame, [this, targetRowOffset]() {
		if (GameRandom::Chance()) {
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.3f);
		}
		else {
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
		}
		FireRow(mRow + targetRowOffset);
		}, true);
	return head;
}

void ThreePeater::PlantUpdate()
{
	// 与其他射手一致，计时和射击动画同时乘生存攻速与雨势行动倍率。
	const float mult = GetAttackSpeedMultiplier();
	mShootTimer += DeltaTime::GetDeltaTime() * mult;
	if (mShootTimer < mShootTime) return;

	// C# 每次 LaunchCounter 到期都会重置；没有目标时也等到下一轮再检查。
	mShootTimer = 0.0f;
	if (!HasTargetInThreeRows()) return;
	StartVolley(mult);
}

bool ThreePeater::HasTargetInRow(int row) const
{
	if (!mBoard || !IsValidRow(row)) return false;

	const float plantX = GetPosition().x;
	bool found = false;
	mBoard->mEntityManager.ForEachZombieInRow(row, [&](Zombie* zombie) {
		if (found || !zombie) return;
		const float zombieX = zombie->GetPosition().x;
		if (!zombie->IsMindControlled()
			&& zombieX >= plantX && zombieX <= SCENE_WIDTH && zombie->HasHead()) {
			found = true;
		}
	});
	return found;
}

bool ThreePeater::HasTargetInThreeRows() const
{
	return HasTargetInRow(mRow)
		|| HasTargetInRow(mRow - 1)
		|| HasTargetInRow(mRow + 1);
}

bool ThreePeater::IsValidRow(int row) const
{
	return mBoard && row >= 0 && row < mBoard->mRows;
}

void ThreePeater::StartVolley(float attackSpeedMultiplier)
{
	const float shootSpeed =
		(kShootFramesPerSecond / kReanimFramesPerSecond) * attackSpeedMultiplier;

	// 三个头始终完整开火；边界外的弹道会在 FireRow 中补偿到本行。
	if (mHeadAnim) {
		mHeadAnim->PlayTrackOnce(
			"anim_shooting1", "anim_head_idle1", shootSpeed, kShootBlendSeconds);
	}
	if (mHeadAnim2) {
		mHeadAnim2->PlayTrackOnce(
			"anim_shooting2", "anim_head_idle2", shootSpeed, kShootBlendSeconds);
	}
	if (mHeadAnim3) {
		mHeadAnim3->PlayTrackOnce(
			"anim_shooting3", "anim_head_idle3", shootSpeed, kShootBlendSeconds);
	}
}

void ThreePeater::ShootBullet()
{
	FireRow(mRow);
}

void ThreePeater::FireRow(int targetRow)
{
	if (!mBoard) return;

	const int sourceRow = mRow;
	const bool isBoundaryCompensation = !IsValidRow(targetRow);
	// 顶、底边缺失的斜路不吞弹，改由本行补发，保证每轮始终有三颗豌豆。
	if (isBoundaryCompensation) {
		targetRow = sourceRow;
	}
	const Vector bulletPosition = GetPosition() + Vector(kBulletOffsetX, kBulletOffsetY);
	Bullet* bullet = mBoard->CreateBullet(BulletType::BULLET_PEA, targetRow, bulletPosition);
	if (bullet && isBoundaryCompensation) {
		bullet->SetVelocityX(kBoundaryCompensationBulletSpeed);
	}
	if (bullet && targetRow != sourceRow) {
		bullet->EnableThreepeaterMotion(sourceRow);
	}
}

void ThreePeater::SaveExtraData(nlohmann::json& j) const
{
	Shooter::SaveExtraData(j);
	SaveHeadAnimatorState(j, "head2Anim", mHeadAnim2.get());
	SaveHeadAnimatorState(j, "head3Anim", mHeadAnim3.get());
}

void ThreePeater::LoadExtraData(const nlohmann::json& j)
{
	Shooter::LoadExtraData(j);
	LoadHeadAnimatorState(j, "head2Anim", mHeadAnim2.get());
	LoadHeadAnimatorState(j, "head3Anim", mHeadAnim3.get());
}

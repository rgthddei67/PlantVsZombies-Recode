#include "DancerZombie.h"
#include "BackupDancerZombie.h"
#include "../Board.h"
#include "../../ParticleSystem/ParticleSystem.h"

namespace {
	constexpr float kDancingInDuration = 2.0f;	// 入场月球漫步基础时长（原版 200cs），另加 0~0.12s 随机
	constexpr float kHoldDuration = 2.0f;		// 响指后定身跳舞时长（原版 200cs）
	constexpr float kDanceLimitX = 700.0f;		// 越过此 x 不再补充召唤（原版 dance limit）
	constexpr float kMoonwalkClip = 2.0f;		// 原版 rate24（12fps 的 2 倍）；截图手感可微调
	constexpr float kPointClip = 2.0f;
	constexpr float kArmraiseClip = 1.8f;		// 与伴舞 kArmraiseClip 保持一致，否则举手段散拍
	constexpr float kDanceAnimSpeed = 1.2f;		// 与伴舞 kDanceAnimSpeed 保持一致
	constexpr float kSummonFrontMinX = 130.0f;	// 舞王 x<130 时不放前方位（原版防出左屏）
	constexpr float kSummonSideDist = 100.0f;	// 同行前/后伴舞与舞王的 x 距离
	constexpr int   kPointDoneFrame = 36;		// anim_point 段(27~36)末帧：响指完成点
}

void DancerZombie::SetupZombie()
{
	mBodyHealth = 500;
	mBodyMaxHealth = 500;
	SetAnimationSpeed(kDanceAnimSpeed);

	if (mIsPreview) { PlayTrack("anim_moonwalk", kMoonwalkClip); return; }

	// 帧号为主人指定：Die@146（anim_death 112~147 内），EatTarget@90/99（anim_eat 78~111 内）
	mAnimator->AddFrameEvent(146, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(90, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(99, [this]() { this->EatTarget(); }, true);

	// anim_point 播毕 → 召唤 + 进 HOLD。帧 36 只存在于 point 段内，其他轨道不会经过；
	// repeating=true + phase 守卫：补位召唤会多次经过此帧。
	mAnimator->AddFrameEvent(kPointDoneFrame, [this]() {
		if (mPhase != DancerPhase::SNAPPING) return;
		SummonBackupDancers();
		mPhase = DancerPhase::HOLD;
		mPhaseTimer = kHoldDuration;
		mLastBeatBucket = -1;
	}, true);

	mPhase = DancerPhase::DANCING_IN;
	mPhaseTimer = kDancingInDuration + GameRandom::Range(0, 12) * 0.01f;
	PlayTrack("anim_moonwalk", kMoonwalkClip);
}

void DancerZombie::ZombieUpdate(float scaledTime)
{
	// 魅惑边沿：放弃旧伴舞（它们继续以原阵营跳舞）；此后新召唤的伴舞在 SummonBackupDancers 里继承魅惑
	if (mIsMindControlled && !mCharmHandled) {
		mCharmHandled = true;
		for (int i = 0; i < 4; ++i) mFollowerID[i] = NULL_ZOMBIE_ID;
	}

	switch (mPhase) {
	case DancerPhase::DANCING_IN:
		mPhaseTimer -= scaledTime;
		if (mPhaseTimer <= 0.0f && mHasHead) {
			mPhase = DancerPhase::SNAPPING;
			PlayTrack("anim_point", kPointClip, 0.3f);
		}
		break;
	case DancerPhase::SNAPPING:
		// 等 anim_point 第 36 帧事件收尾
		break;
	case DancerPhase::HOLD:
		mPhaseTimer -= scaledTime;
		UpdateDanceTrack(0.3f);
		if (mPhaseTimer <= 0.0f) {
			mPhase = DancerPhase::DANCING;
		}
		break;
	case DancerPhase::DANCING:
		UpdateDanceTrack(0.3f);
		// 补位：只在节拍 12（walk→armraise 切拍点，原版编排）、有头、未越界、确实缺人时再打响指
		if (mHasHead && mBoard && mBoard->GetDanceBeatFrame() == 12
			&& GetPosition().x < kDanceLimitX && NeedsMoreBackupDancers()) {
			mPhase = DancerPhase::SNAPPING;
			PlayTrack("anim_point", kPointClip, 0.3f);
		}
		break;
	}
}

void DancerZombie::UpdateDanceTrack(float blendTime)
{
	if (!mBoard || mIsDying) return;
	const int bucket = (mBoard->GetDanceBeatFrame() >= 12) ? 1 : 0;
	if (bucket == mLastBeatBucket) return;
	mLastBeatBucket = bucket;
	if (bucket == 1)
		PlayTrack("anim_armraise", kArmraiseClip, blendTime);
	else
		PlayTrack("anim_walk", 0.0f, blendTime);
}

void DancerZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	// 打响指/定身跳舞阶段不移动；入场(moonwalk)与齐舞阶段按轨道 _ground 速度正常推进
	if (mPhase == DancerPhase::SNAPPING || mPhase == DancerPhase::HOLD) return;
	Zombie::ZombieMove(scaledDelta, transform);
}

void DancerZombie::PlayWalkAnimation(float blendTime)
{
	switch (mPhase) {
	case DancerPhase::DANCING_IN:
		PlayTrack("anim_moonwalk", kMoonwalkClip, blendTime);
		return;
	case DancerPhase::SNAPPING:
		PlayTrack("anim_point", kPointClip, blendTime);
		return;
	default:
		mLastBeatBucket = -1;
		if (mBoard && mBoard->GetDanceBeatFrame() >= 12)
			PlayTrack("anim_armraise", kArmraiseClip, blendTime);
		else
			PlayTrack("anim_walk", 0.0f, blendTime);
	}
}

void DancerZombie::StartEat(ColliderComponent* other)
{
	Zombie::StartEat(other);
	// 原版：入场月球漫步中被迫开吃 → 啃完立即打响指（计时清零；啃食期间状态机本就暂停）
	if (mIsEating && mPhase == DancerPhase::DANCING_IN) {
		mPhaseTimer = 0.0f;
	}
}

void DancerZombie::SummonBackupDancers()
{
	if (!mBoard || !mHasHead) return;
	const float x = GetPosition().x;
	const struct { int row; float posX; } slots[4] = {
		{ mRow - 1, x },                      // 上一行同列
		{ mRow + 1, x },                      // 下一行同列
		{ mRow,     x - kSummonSideDist },    // 同行前方（太靠左跳过）
		{ mRow,     x + kSummonSideDist },    // 同行后方
	};
	for (int i = 0; i < 4; ++i) {
		// 槽位有效 = 活着且与领队同阵营（伴舞被魅惑后不清领队侧槽位，只能在此按阵营判失效）
		if (Zombie* f = mBoard->mEntityManager.GetZombie(mFollowerID[i])) {
			if (f->IsMindControlled() == mIsMindControlled) continue;
		}
		mFollowerID[i] = NULL_ZOMBIE_ID;
		if (slots[i].row < 0 || slots[i].row >= mBoard->mRows) continue;
		if (i == 2 && x < kSummonFrontMinX) continue;
		Zombie* summoned = mBoard->CreateZombie(ZombieType::ZOMBIE_BACKUP_DANCER,
			slots[i].row, slots[i].posX);
		if (!summoned) continue;
		mFollowerID[i] = summoned->mZombieID;
		if (auto* backup = dynamic_cast<BackupDancerZombie*>(summoned)) {
			backup->mLeaderID = mZombieID;
		}
		if (mIsMindControlled) summoned->StartMindControlled();	// 魅惑领队的新伴舞继承阵营
	}
}

bool DancerZombie::NeedsMoreBackupDancers() const
{
	if (!mBoard) return false;
	for (int i = 0; i < 4; ++i) {
		// 永久不可用的位不算缺：行越界（上/下行）与太靠左的前方位
		//（原版对前方位无此豁免，会在 x<130 时无限重打响指——这里补上防死循环）
		if (i == 0 && mRow - 1 < 0) continue;
		if (i == 1 && mRow + 1 >= mBoard->mRows) continue;
		if (i == 2 && GetPosition().x < kSummonFrontMinX) continue;
		Zombie* follower = mBoard->mEntityManager.GetZombie(mFollowerID[i]);
		// 死亡或阵营不同（被魅惑脱队）都算缺人
		if (!follower || follower->IsMindControlled() != mIsMindControlled) return true;
	}
	return false;
}

void DancerZombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieJacksonHeadOff", GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void DancerZombie::ArmDrop()
{
	if (!mHasArm) return;
	// 同伴舞：藏小臂+手、保大臂，无材质替换、无粒子（主人指定；MJ 版 reanim 无残肢轨道）
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void DancerZombie::ZombieItemUpdate() const
{
	if (!mHasArm) {
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	}
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
		mAnimator->SetTrackVisible("anim_hair", false);
	}
}

void DancerZombie::SaveExtraData(nlohmann::json& j) const
{
	j["phase"] = static_cast<int>(mPhase);
	j["phaseTimer"] = mPhaseTimer;
	j["charmHandled"] = mCharmHandled;
	j["followers"] = nlohmann::json::array({
		mFollowerID[0], mFollowerID[1], mFollowerID[2], mFollowerID[3] });
}

void DancerZombie::LoadExtraData(const nlohmann::json& j)
{
	mPhase = static_cast<DancerPhase>(j.value("phase", 0));
	mPhaseTimer = j.value("phaseTimer", 0.0f);
	mCharmHandled = j.value("charmHandled", false);
	if (j.contains("followers") && j["followers"].is_array()) {
		const auto& arr = j["followers"];
		for (int i = 0; i < 4 && i < static_cast<int>(arr.size()); ++i) {
			mFollowerID[i] = arr[i].get<int>();
		}
	}
	// 读档僵尸 ID 经 CreateZombieWithID 保值，followers 交叉引用安全（死者 GetZombie 返回 null 即空位）
	if (mIsEating) return;
	if (mPhase == DancerPhase::DANCING_IN || mPhase == DancerPhase::SNAPPING) return;
	// HOLD/DANCING：下帧按节拍重刷轨道（RestoreAnimState 的帧位被重新入拍覆盖，预期行为）
	mLastBeatBucket = -1;
}

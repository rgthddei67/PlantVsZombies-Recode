#include "GloomShroom.h"
#include "../../ParticleSystem/ParticleSystem.h"

#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "../../ResourceKeys.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
	constexpr float kAttackInterval = 2.0f; // 原版 launch rate 200cs，单位：游戏秒
	constexpr float kOriginalShootingFps = 14.0f; // 原版 anim_shooting 播放帧率，单位：帧/秒
	constexpr float kConvertedReanimFps = 12.0f; // 主人转换版 GloomShroom.reanim 的基础帧率
	constexpr float kShootingClipSpeed = kOriginalShootingFps / kConvertedReanimFps; // 还原原版射击轨道速度
	constexpr int kPulseDamage = 20; // 每次环形伤害的基础值
	constexpr float kHorizontalRadiusInCells = 1.5f; // 原版 240px 攻击矩形相对格中心的左右半径
	constexpr std::array<float, 4> kCloudTimes = { 0.64f, 0.92f, 1.20f, 1.48f }; // 原版 136/108/80/52cs
	constexpr std::array<float, 4> kDamageTimes = { 0.74f, 1.02f, 1.30f, 1.58f }; // 原版 126/98/70/42cs

	int CountReachedEvents(const std::array<float, 4>& times, float elapsed)
	{
		return static_cast<int>(std::count_if(times.begin(), times.end(),
			[elapsed](float time) { return time <= elapsed; }));
	}
}

void GloomShroom::PlantUpdate()
{
	const float attackSpeedMultiplier = GetAttackSpeedMultiplier();
	const float scaledDeltaTime = DeltaTime::GetDeltaTime() * attackSpeedMultiplier;
	mShootTimer += scaledDeltaTime;

	if (mAttacking) {
		// 天气或词条在攻击中途变化时，动画与四段逻辑仍保持同一倍率。
		SetClipSpeed(kShootingClipSpeed * attackSpeedMultiplier);
		AdvanceAttack(scaledDeltaTime);
	}

	if (!mAttacking && mShootTimer >= kAttackInterval) {
		// 原版即使本轮没有目标也会重新安排下一轮索敌，而不是永久保持蓄满。
		mShootTimer = 0.0f;
		if (HasTargetInRange()) {
			StartAttack(attackSpeedMultiplier);
		}
	}
}

bool GloomShroom::HasTargetInRange() const
{
	if (!mBoard) return false;
	bool found = false;
	for (int row = std::max(0, mRow - 1);
		row <= std::min(mBoard->mRows - 1, mRow + 1) && !found; ++row) {
		mBoard->mEntityRegistry.ForEachZombieInRow(row, [&](Zombie* zombie) {
			if (!found && IsTargetInRange(zombie)) found = true;
		});
	}
	return found;
}

bool GloomShroom::IsTargetInRange(Zombie* zombie) const
{
	if (!mBoard || !zombie || !zombie->IsActive() || zombie->IsDying()
		|| zombie->IsMindControlled()
		|| !mBoard->CanPlantAcquireZombie(this, zombie)) {
		return false;
	}

	auto* collider = zombie->GetColliderComponent();
	if (!collider || !collider->mEnabled) return false;
	const SDL_FRect bounds = collider->GetBoundingBox();
	const float radius = CELL_COLLIDER_SIZE_X * kHorizontalRadiusInCells;
	const float left = GetPosition().x - radius;
	const float right = GetPosition().x + radius;
	return bounds.x < right && bounds.x + bounds.w > left;
}

void GloomShroom::StartAttack(float attackSpeedMultiplier)
{
	mAttacking = true;
	mAttackElapsed = 0.0f;
	mNextCloudIndex = 0;
	mNextDamageIndex = 0;
	PlayTrackOnce("anim_shooting", "anim_idle",
		kShootingClipSpeed * attackSpeedMultiplier, 0.2f);
}

void GloomShroom::AdvanceAttack(float scaledDeltaTime)
{
	mAttackElapsed = std::min(kAttackInterval, mAttackElapsed + scaledDeltaTime);

	// 倍速或卡顿可能在一帧跨过多个事件；按原版时间顺序逐个补结算。
	while (mNextCloudIndex < static_cast<int>(kCloudTimes.size())
		|| mNextDamageIndex < static_cast<int>(kDamageTimes.size())) {
		const float cloudTime = mNextCloudIndex < static_cast<int>(kCloudTimes.size())
			? kCloudTimes[mNextCloudIndex] : kAttackInterval + 1.0f;
		const float damageTime = mNextDamageIndex < static_cast<int>(kDamageTimes.size())
			? kDamageTimes[mNextDamageIndex] : kAttackInterval + 1.0f;
		const float nextTime = std::min(cloudTime, damageTime);
		if (nextTime > mAttackElapsed) break;

		if (cloudTime <= damageTime) {
			EmitCloud();
			++mNextCloudIndex;
		}
		else {
			ApplyDamagePulse();
			++mNextDamageIndex;
		}
	}

	if (mAttackElapsed >= kAttackInterval) {
		mAttacking = false;
		mAttackElapsed = 0.0f;
		mNextCloudIndex = 0;
		mNextDamageIndex = 0;
	}
}

void GloomShroom::EmitCloud() const
{
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("GloomCloud", GetVisualAnchorPosition(),
			LAYER_EFFECTS_WORLD);
	}
}

void GloomShroom::ApplyDamagePulse() const
{
	if (!mBoard) return;
	for (int row = std::max(0, mRow - 1);
		row <= std::min(mBoard->mRows - 1, mRow + 1); ++row) {
		mBoard->mEntityRegistry.ForEachZombieInRow(row, [&](Zombie* zombie) {
			if (!IsTargetInRange(zombie)) return;

			// 加固门只改变自身这一击的盾牌语义；环形云雾不会被它截断其他方向。
			const bool blocksFume = zombie->BlocksFumePiercing();
			const int damage = zombie->ModifyFumeDamage(kPulseDamage);
			zombie->TakeDamage(damage, DamageSource::PLANT,
				/*penetrateShield=*/!blocksFume,
				/*discardShieldOverflow=*/blocksFume);
			AudioSystem::PlaySound(
				ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY1, 0.2f);
		});
	}
}

void GloomShroom::SaveExtraData(nlohmann::json& j) const
{
	j["shootTimer"] = mShootTimer;
	j["attacking"] = mAttacking;
	j["attackElapsed"] = mAttackElapsed;
	j["nextCloudIndex"] = mNextCloudIndex;
	j["nextDamageIndex"] = mNextDamageIndex;
}

void GloomShroom::LoadExtraData(const nlohmann::json& j)
{
	const float savedShootTimer = j.value("shootTimer", 0.0f);
	mShootTimer = std::isfinite(savedShootTimer)
		? std::clamp(savedShootTimer, 0.0f, kAttackInterval) : 0.0f;
	mAttacking = j.value("attacking", false);

	const float savedAttackElapsed = j.value("attackElapsed", 0.0f);
	mAttackElapsed = std::isfinite(savedAttackElapsed)
		? std::clamp(savedAttackElapsed, 0.0f, kAttackInterval) : 0.0f;
	if (!mAttacking || mAttackElapsed >= kAttackInterval) {
		mAttacking = false;
		mAttackElapsed = 0.0f;
		mNextCloudIndex = 0;
		mNextDamageIndex = 0;
		return;
	}

	// 已跨过的事件即使旧档索引缺失也不重放；显式索引只允许继续向后推进。
	mNextCloudIndex = std::clamp(j.value("nextCloudIndex", 0),
		CountReachedEvents(kCloudTimes, mAttackElapsed),
		static_cast<int>(kCloudTimes.size()));
	mNextDamageIndex = std::clamp(j.value("nextDamageIndex", 0),
		CountReachedEvents(kDamageTimes, mAttackElapsed),
		static_cast<int>(kDamageTimes.size()));
}

void GloomShroom::SetShootCycleForTesting(float elapsedSeconds)
{
	mShootTimer = std::isfinite(elapsedSeconds)
		? std::clamp(elapsedSeconds, 0.0f, kAttackInterval) : 0.0f;
	mAttacking = false;
	mAttackElapsed = 0.0f;
	mNextCloudIndex = 0;
	mNextDamageIndex = 0;
	if (!GetSleepState()) PlayTrack("anim_idle");
}

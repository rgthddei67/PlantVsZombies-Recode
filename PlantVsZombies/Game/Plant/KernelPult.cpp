#include "KernelPult.h"

#include "../Board.h"
#include "../Bullet/Bullet.h"
#include "../Zombie/Zombie.h"
#include "../ShadowComponent.h"

#include <algorithm>
#include <limits>

namespace {
	constexpr int kFireFrame = 30;                         // 主人确认的 AddFrameEvent 真实发射帧
	constexpr float kReanimFramesPerSecond = 12.0f;        // Cornpult.reanim 的基础帧率
	constexpr float kShootFramesPerSecond = 35.0f;         // C# anim_shooting 的播放帧率
	constexpr float kIdleFramesPerSecondMin = 10.0f;       // 经典普通植物待机随机帧率下限
	constexpr float kIdleFramesPerSecondMax = 15.0f;       // 经典普通植物待机随机帧率上限
	constexpr float kShootBlendSeconds = 0.0f;             // 射击动画进入与返回的混合时间
	constexpr float kInitialShootInterval = 3.0f;          // C# launch rate 300 厘秒
	constexpr float kRepeatShootIntervalMin = 2.86f;       // 300-NextNumber(15) 的最短后续周期
	constexpr float kRepeatShootIntervalMax = 3.0f;        // 300-NextNumber(15) 的最长后续周期
	constexpr int kButterChanceDenominator = 4;            // 每四次独立攻击平均一次黄油
	const Vector kKernelLaunchOffset(-7.0f, -70.0f);        // C# 玉米粒左上坐标换算到本项目稳定视觉锚点的弹心偏移
	const Vector kButterLaunchOffset(-14.0f, -89.0f);       // C# 黄油左上坐标换算到本项目稳定视觉锚点的弹心偏移
	constexpr float kFlightDuration = 1.2f;                // 经典投掷物约 120 厘秒的飞行时间
	constexpr float kArcApexHeight = 210.0f;               // 相对起终点线性插值轨迹的最高拱高，单位 px
	constexpr float kTargetHeightRatio = 0.35f;            // 瞄准碰撞箱上部身体，避开脚底与头顶空白
	constexpr float kFallbackLandingOffsetY = -20.0f;      // 目标消失时相对同行地形基线的安全落点高度
	constexpr int kKernelDamage = 20;                      // C# ProjectileType.Kernel 直击伤害
	constexpr int kButterDamage = 40;                      // C# ProjectileType.Butter 直击伤害
	constexpr float kShootSoundVolume = 0.3f;              // Throw/Throw2 发射 Foley 音量
}

void KernelPult::SetupPlant()
{
	Plant::SetupPlant();
	if (!mAnimator) return;

	mAnimator->PlayTrack("anim_idle");
	mAnimator->SetSpeed(GameRandom::Range(
		kIdleFramesPerSecondMin / kReanimFramesPerSecond,
		kIdleFramesPerSecondMax / kReanimFramesPerSecond));
	mShootInterval = kInitialShootInterval;
	mShootTimer = GameRandom::Range(0.0f, kInitialShootInterval);
	mButterShotPending = false;
	ApplyHeldProjectileVisual();

	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetOffset(Vector(2.0f, 24.0f));
	}

	mAnimator->AddFrameEvent(kFireFrame, [this]() {
		if (GetCurrentTrackName() == "anim_shooting") FireProjectile();
	}, true);
}

void KernelPult::PlantUpdate()
{
	const float attackSpeed = GetAttackSpeedMultiplier();
	mShootTimer += DeltaTime::GetDeltaTime() * attackSpeed;
	if (mShootTimer < mShootInterval) return;

	// 到期即开始下一周期；无目标时不会每个逻辑帧重复遍历行桶。
	mShootTimer = 0.0f;
	mShootInterval = GameRandom::Range(
		kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	if (!FindTarget() || !mAnimator) return;

	BeginShot();
	const float shootSpeed =
		(kShootFramesPerSecond / kReanimFramesPerSecond) * attackSpeed;
	mAnimator->PlayTrackOnce(
		"anim_shooting", "anim_idle", shootSpeed,
		kShootBlendSeconds, 0.0f, kShootBlendSeconds);
}

Zombie* KernelPult::FindTarget() const
{
	if (!mBoard) return nullptr;

	Zombie* closest = nullptr;
	float closestX = std::numeric_limits<float>::max();
	const float plantX = GetPosition().x;
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (!zombie || !zombie->IsActive() || zombie->IsDying()
			|| zombie->IsMindControlled() || !zombie->HasHead()
			|| !mBoard->CanPlantAcquireZombie(this, zombie)) {
			return;
		}
		const ColliderComponent* collider = zombie->GetColliderComponent();
		if (!collider) return;
		const SDL_FRect bounds = collider->GetBoundingBox();
		const float centerX = bounds.x + bounds.w * 0.5f;
		if (bounds.x + bounds.w < plantX || centerX > static_cast<float>(SCENE_WIDTH)) {
			return;
		}
		if (centerX < closestX) {
			closestX = centerX;
			closest = zombie;
		}
	});
	return closest;
}

void KernelPult::ApplyHeldProjectileVisual()
{
	if (!mAnimator) return;
	mAnimator->SetTrackVisible("Cornpult_kernal", !mButterShotPending);
	mAnimator->SetTrackVisible("Cornpult_butter", mButterShotPending);
}

void KernelPult::BeginShot()
{
	if (mForcedShotForTesting >= 0) {
		mButterShotPending = mForcedShotForTesting != 0;
		mForcedShotForTesting = -1;
	}
	else {
		mButterShotPending = GameRandom::Range(
			0, kButterChanceDenominator - 1) == 0;
	}
	ApplyHeldProjectileVisual();
}

void KernelPult::FireProjectile()
{
	if (!mBoard || mIsPreview) return;

	const bool fireButter = mButterShotPending;
	const Vector launchPosition = GetVisualAnchorPosition()
		+ (fireButter ? kButterLaunchOffset : kKernelLaunchOffset);
	Vector landingPosition(
		static_cast<float>(SCENE_WIDTH + 20),
		mBoard->GetRowCenterYAtX(mRow, static_cast<float>(SCENE_WIDTH))
			+ kFallbackLandingOffsetY);
	if (Zombie* target = FindTarget()) {
		if (const ColliderComponent* collider = target->GetColliderComponent()) {
			const SDL_FRect bounds = collider->GetBoundingBox();
			const float currentTargetX = bounds.x + bounds.w * 0.5f;
			landingPosition.x = target->GetTargetLeadX(kFlightDuration);
			// 屋顶水平移动会同步改变目标高度；沿 Board 连续坡面补偿预测后的 Y。
			const float terrainDeltaY =
				mBoard->GetRowCenterYAtX(mRow, landingPosition.x)
				- mBoard->GetRowCenterYAtX(mRow, currentTargetX);
			landingPosition.y = bounds.y + bounds.h * kTargetHeightRatio
				+ terrainDeltaY;
		}
	}

	AudioSystem::PlaySound(GameRandom::Chance()
		? ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT
		: ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, kShootSoundVolume);
	Bullet* projectile = mBoard->CreateBullet(
		fireButter ? BulletType::BULLET_BUTTER : BulletType::BULLET_KERNEL,
		mRow, launchPosition);
	if (projectile) {
		projectile->SetBulletDamage(fireButter ? kButterDamage : kKernelDamage);
		projectile->ConfigureLobbedMotion(
			landingPosition, kFlightDuration, kArcApexHeight);
	}

	// C# 在发射节点立即恢复手持玉米粒；即使对象池创建失败也不能把黄油留在植株上。
	mButterShotPending = false;
	ApplyHeldProjectileVisual();
}

void KernelPult::SaveExtraData(nlohmann::json& j) const
{
	j["shootTimer"] = mShootTimer;
	j["shootInterval"] = mShootInterval;
	j["butterShotPending"] = mButterShotPending;
}

void KernelPult::LoadExtraData(const nlohmann::json& j)
{
	mShootInterval = std::clamp(
		j.value("shootInterval", kInitialShootInterval),
		kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	mShootTimer = std::clamp(
		j.value("shootTimer", 0.0f), 0.0f, mShootInterval);
	mButterShotPending = j.value("butterShotPending", false);
	ApplyHeldProjectileVisual();
}

void KernelPult::SetShootCycleForTesting(
	float elapsedSeconds, float intervalSeconds, int forcedShot)
{
	mShootInterval = std::clamp(
		intervalSeconds, kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	mShootTimer = std::clamp(elapsedSeconds, 0.0f, mShootInterval);
	mForcedShotForTesting = std::clamp(forcedShot, -1, 1);
}

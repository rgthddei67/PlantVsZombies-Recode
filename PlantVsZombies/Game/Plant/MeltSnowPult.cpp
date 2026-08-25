#include "MeltSnowPult.h"
#include "../../GameApp.h"
#include "../../ResourceManager.h"

#include "../Board.h"
#include "../Bullet/Bullet.h"
#include "../Zombie/Zombie.h"
#include "../IceWall.h"
#include "../ShadowComponent.h"

#include <algorithm>
#include <limits>

namespace {
	constexpr int kFireFrame = 43;                         // 主人确认的 AddFrameEvent 真实发射帧
	constexpr float kReanimFramesPerSecond = 12.0f;        // MeltSnowPult.reanim 的基础帧率
	constexpr float kShootFramesPerSecond = 35.0f;         // 沿用 CabbagePult anim_shooting 的播放帧率
	constexpr float kIdleFramesPerSecondMin = 10.0f;       // 经典普通植物待机随机帧率下限
	constexpr float kIdleFramesPerSecondMax = 15.0f;       // 经典普通植物待机随机帧率上限
	constexpr float kShootBlendSeconds = 0.0f;             // 射击动画进入与返回的混合时间
	constexpr float kInitialShootInterval = 3.0f;          // 初次攻击周期，单位：游戏秒
	constexpr float kRepeatShootIntervalMin = 2.86f;       // 后续攻击周期随机下限，单位：游戏秒
	constexpr float kRepeatShootIntervalMax = 3.0f;        // 后续攻击周期随机上限，单位：游戏秒
	constexpr int kSaltAmmoCapacity = 3;                   // 每次准确寒潮预报把库存直接补到的上限
	const Vector kLaunchOffset(-21.0f, -45.0f);             // 沿用卷心菜投手稳定视觉锚点的弹心偏移
	constexpr float kFlightDuration = 1.2f;                // 经典投掷物飞行时间，单位：秒
	constexpr float kArcApexHeight = 210.0f;               // 相对起终点线性插值轨迹的最高拱高，单位：px
	constexpr float kTargetHeightRatio = 0.35f;            // 瞄准碰撞箱上部身体，避开脚底与头顶空白
	constexpr float kFallbackLandingOffsetY = -20.0f;      // 目标消失时相对同行地形基线的安全落点高度
	constexpr int kProjectileDamage = 20;                  // 普通雪团与盐晶弹共同的直击伤害
	constexpr float kShootSoundVolume = 0.3f;              // Throw/Throw2 发射 Foley 音量
	constexpr const char* kHeldProjectileTrack = "MeltSnowPult_snowclod"; // 单轨道运行时换图名称
}

void MeltSnowPult::SetupPlant()
{
	Plant::SetupPlant();
	if (!mAnimator) return;

	mAnimator->PlayTrack("anim_idle");
	mAnimator->SetSpeed(GameRandom::Range(
		kIdleFramesPerSecondMin / kReanimFramesPerSecond,
		kIdleFramesPerSecondMax / kReanimFramesPerSecond));
	mShootInterval = kInitialShootInterval;
	mShootTimer = GameRandom::Range(0.0f, kInitialShootInterval);
	mObservedColdWaveForecast = mBoard && mBoard->HasColdWaveForecast();
	mSaltAmmo = mObservedColdWaveForecast ? kSaltAmmoCapacity : 0;
	mSaltShotPending = false;
	ApplyHeldProjectileVisual();

	if (auto* shadow = GetShadow()) {
		shadow->SetOffset(Vector(2.0f, 24.0f));
	}

	mAnimator->AddFrameEvent(kFireFrame, [this]() {
		if (GetCurrentTrackName() == "anim_shooting") FireProjectile();
	}, true);
}

void MeltSnowPult::PlantUpdate()
{
	UpdateForecastAmmo();
	const float attackSpeed = GetAttackSpeedMultiplier();
	mShootTimer += DeltaTime::GetDeltaTime() * attackSpeed;
	if (mShootTimer < mShootInterval) return;

	// 到期即开始下一周期；无目标时不会每个逻辑帧重复遍历行桶。
	mShootTimer = 0.0f;
	mShootInterval = GameRandom::Range(
		kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	if ((!FindIceWallTarget() && !FindTarget()) || !mAnimator) return;

	BeginShot();
	const float shootSpeed =
		(kShootFramesPerSecond / kReanimFramesPerSecond) * attackSpeed;
	mAnimator->PlayTrackOnce(
		"anim_shooting", "anim_idle", shootSpeed,
		kShootBlendSeconds, 0.0f, kShootBlendSeconds);
}

void MeltSnowPult::UpdateForecastAmmo()
{
	const bool hasForecast = mBoard && mBoard->HasColdWaveForecast();
	if (hasForecast && !mObservedColdWaveForecast) {
		mSaltAmmo = kSaltAmmoCapacity;
		mObservedColdWaveForecast = true;
		ApplyHeldProjectileVisual();
	}
	else if (!hasForecast) {
		mObservedColdWaveForecast = false;
	}
}

Zombie* MeltSnowPult::FindTarget() const
{
	if (!mBoard) return nullptr;

	Zombie* closest = nullptr;
	float closestX = std::numeric_limits<float>::max();
	const float plantX = GetPosition().x;
	mBoard->mEntityRegistry.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (!zombie || !zombie->IsActive() || zombie->IsDying()
			|| zombie->IsMindControlled() || !zombie->HasHead()
			|| !mBoard->CanPlantAcquireZombie(this, zombie)) {
			return;
		}
		const ColliderComponent* collider = zombie->GetColliderComponent();
		if (!collider) return;
		const SDL_FRect bounds = collider->GetBoundingBox();
		const float centerX = bounds.x + bounds.w * 0.5f;
		if (bounds.x + bounds.w < plantX
			|| centerX > static_cast<float>(SCENE_WIDTH)) return;
		if (centerX < closestX) {
			closestX = centerX;
			closest = zombie;
		}
	});
	return closest;
}

IceWall* MeltSnowPult::FindIceWallTarget() const
{
	return mBoard ? mBoard->GetIceWallInRow(mRow) : nullptr;
}

void MeltSnowPult::BeginShot()
{
	if (mForcedShotForTesting >= 0) {
		mSaltShotPending = mForcedShotForTesting != 0;
		mForcedShotForTesting = -1;
		if (mSaltShotPending && mSaltAmmo > 0) --mSaltAmmo;
	}
	else {
		mSaltShotPending = mSaltAmmo > 0;
		if (mSaltShotPending) --mSaltAmmo;
	}
	ApplyHeldProjectileVisual();
}

void MeltSnowPult::ApplyHeldProjectileVisual()
{
	if (!mAnimator) return;
	const bool showSalt = mSaltShotPending || mSaltAmmo > 0;
	const std::string& textureKey = showSalt
		? ResourceKeys::Textures::IMAGE_MELTSNOWPULT_SALTCRYSTAL
		: ResourceKeys::Textures::IMAGE_REANIM_MELTSNOWPULT_SNOWCLOD;
	const Texture* texture = ResourceManager::GetInstance().GetTexture(textureKey);
	mAnimator->SetTrackImage(kHeldProjectileTrack, texture);
}

void MeltSnowPult::FireProjectile()
{
	if (!mBoard || mIsPreview) return;

	const bool fireSalt = mSaltShotPending;
	const Vector launchPosition = GetVisualAnchorPosition() + kLaunchOffset;
	Vector landingPosition(
		static_cast<float>(SCENE_WIDTH + 20),
		mBoard->GetRowCenterYAtX(mRow, static_cast<float>(SCENE_WIDTH))
			+ kFallbackLandingOffsetY);
	bool targetsIceWall = false;
	if (IceWall* wall = FindIceWallTarget()) {
		landingPosition = wall->GetProjectileAimPosition();
		targetsIceWall = true;
	}
	else if (Zombie* target = FindTarget()) {
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
		fireSalt ? BulletType::BULLET_SALT_CRYSTAL
			: BulletType::BULLET_MELT_SNOW,
		mRow, launchPosition);
	if (projectile) {
		projectile->SetBulletDamage(kProjectileDamage);
		projectile->ConfigureLobbedMotion(
			landingPosition, kFlightDuration, kArcApexHeight, targetsIceWall);
	}

	// 发射节点立即恢复“下一发”手持表现；对象池失败也不能留下已消费的盐晶。
	mSaltShotPending = false;
	ApplyHeldProjectileVisual();
}

void MeltSnowPult::OnColdWaveForecastDisrupted()
{
	mSaltAmmo = 0;
	mSaltShotPending = false;
	mObservedColdWaveForecast = false;
	ApplyHeldProjectileVisual();
}

void MeltSnowPult::SaveExtraData(nlohmann::json& j) const
{
	j["shootTimer"] = mShootTimer;
	j["shootInterval"] = mShootInterval;
	j["saltAmmo"] = mSaltAmmo;
	j["saltShotPending"] = mSaltShotPending;
	j["observedColdWaveForecast"] = mObservedColdWaveForecast;
}

void MeltSnowPult::LoadExtraData(const nlohmann::json& j)
{
	mShootInterval = std::clamp(
		j.value("shootInterval", kInitialShootInterval),
		kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	mShootTimer = std::clamp(
		j.value("shootTimer", 0.0f), 0.0f, mShootInterval);
	mSaltAmmo = std::clamp(j.value("saltAmmo", 0), 0, kSaltAmmoCapacity);
	mSaltShotPending = j.value("saltShotPending", false);
	mObservedColdWaveForecast = j.value("observedColdWaveForecast", false);
	ApplyHeldProjectileVisual();
}

void MeltSnowPult::SetShootCycleForTesting(
	float elapsedSeconds, float intervalSeconds, int forcedShot)
{
	mShootInterval = std::clamp(
		intervalSeconds, kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	mShootTimer = std::clamp(elapsedSeconds, 0.0f, mShootInterval);
	mForcedShotForTesting = std::clamp(forcedShot, -1, 1);
}

void MeltSnowPult::SetSaltStateForTesting(
	int ammo, bool pending, bool observedForecast)
{
	mSaltAmmo = std::clamp(ammo, 0, kSaltAmmoCapacity);
	mSaltShotPending = pending;
	mObservedColdWaveForecast = observedForecast;
	ApplyHeldProjectileVisual();
}

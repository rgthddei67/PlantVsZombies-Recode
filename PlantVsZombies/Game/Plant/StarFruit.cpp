#include "StarFruit.h"

#include "Game/Board/Board.h"
#include "../Bullet/Bullet.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

#include <SDL2/SDL.h>
#include <array>
#include <cmath>

namespace {
	constexpr int kFireFrame = 27;                         // 主人给出的 AddFrameEvent 真实帧号，代码直接使用
	constexpr float kReanimFramesPerSecond = 12.0f;        // Starfruit.reanim 的基础帧率
	constexpr float kShootFramesPerSecond = 28.0f;         // C# PlayBodyReanim(anim_shoot) 的播放帧率
	constexpr float kIdleFramesPerSecondMin = 10.0f;       // C# 普通整株 reanim 待机随机帧率下限
	constexpr float kIdleFramesPerSecondMax = 15.0f;       // C# 普通整株 reanim 待机随机帧率上限
	constexpr float kShootBlendSeconds = 0.2f;             // C# StartBlend(20) 的进入与返回混合时间
	constexpr float kInitialShootInterval = 1.5f;          // C# Starfruit launch rate 150 厘秒
	constexpr float kRepeatShootIntervalMin = 1.36f;       // 150-NextNumber(15) 的最短后续周期
	constexpr float kRepeatShootIntervalMax = 1.5f;        // 150-NextNumber(15) 的最长后续周期
	constexpr float kTargetOriginOffsetY = -10.0f;         // C# mY+40 换算到本项目格子中心后的索敌 Y
	constexpr float kBulletOffsetX = -15.0f;               // C# mX+25 换算到本项目格子中心后的发射 X
	constexpr float kBulletOffsetY = -25.0f;               // C# mY+25 换算到本项目格子中心后的发射 Y
	constexpr float kStarSpeed = 333.0f;                   // 原版 3.33px/厘秒换算后的星弹速度，单位 px/s
	constexpr float kDiagonalDegrees = 30.0f;              // 两条向右斜线与水平线的夹角
	constexpr float kPi = 3.14159265358979323846f;         // 角度与弧度换算常量
	constexpr int kStarDamage = 20;                        // 原版单颗星弹基础伤害
}

void StarFruit::SetupPlant()
{
	Plant::SetupPlant();
	// 原版杨桃资源本体自带贴地明暗，不再叠加通用植物影子。
	RemoveShadow();
	if (!mAnimator) return;

	mAnimator->PlayTrack("anim_idle");
	mAnimator->SetSpeed(GameRandom::Range(
		kIdleFramesPerSecondMin / kReanimFramesPerSecond,
		kIdleFramesPerSecondMax / kReanimFramesPerSecond));

	// 原版首轮 launch counter 在 0..150 厘秒随机；用已累计时间表达同一均匀分布。
	mShootInterval = kInitialShootInterval;
	mShootTimer = GameRandom::Range(0.0f, kInitialShootInterval);

	mAnimator->AddFrameEvent(kFireFrame, [this]() {
		FireStarVolley();
	}, true);
}

void StarFruit::PlantUpdate()
{
	const float attackSpeed = GetAttackSpeedMultiplier();
	mShootTimer += DeltaTime::GetDeltaTime() * attackSpeed;
	if (mShootTimer < mShootInterval) return;

	// C# 到期时无论能否找到目标都会开始下一轮，避免无目标期间每帧重复扫描全场行桶。
	mShootTimer = 0.0f;
	mShootInterval = GameRandom::Range(
		kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	if (!HasStarFruitTarget() || !mAnimator) return;

	const float shootSpeed =
		(kShootFramesPerSecond / kReanimFramesPerSecond) * attackSpeed;
	mAnimator->PlayTrackOnce(
		"anim_shoot", "anim_idle", shootSpeed,
		kShootBlendSeconds, 0.0f, kShootBlendSeconds);
}

bool StarFruit::HasStarFruitTarget() const
{
	if (!mBoard) return false;
	// C# 的 recently eaten 守卫让杨桃在刚被咬时继续完成开火；现有 eaterCount 是正式啃食抓手。
	if (mEaterCount > 0) return true;

	const Vector targetOrigin = GetPosition() + Vector(0.0f, kTargetOriginOffsetY);
	bool found = false;
	for (int row = 0; row < mBoard->mRows && !found; ++row) {
		mBoard->mEntityRegistry.ForEachZombieInRow(row, [&](Zombie* zombie) {
			if (found || !zombie || !zombie->IsActive() || zombie->IsDying()
				|| zombie->IsMindControlled() || !zombie->HasHead()
				|| !mBoard->CanPlantAcquireZombie(this, zombie)) {
				return;
			}
			// 原版 Boss 占据多行；杨桃在后四列时直接获得开火资格，不套普通弹道窗口。
			if (zombie->mZombieType == ZombieType::ZOMBIE_BOSS && mColumn >= 5) {
				found = true;
				return;
			}

			const ColliderComponent* collider = zombie->GetColliderComponent();
			if (!collider) return;
			SDL_FRect bounds = collider->GetBoundingBox();
			if (row == mRow) {
				// 五向中的水平星只向左飞；同行目标必须整个位于杨桃左侧。
				found = bounds.x + bounds.w < targetOrigin.x;
				return;
			}

			if (zombie->mZombieType == ZombieType::ZOMBIE_DIGGER
				|| zombie->mZombieType == ZombieType::ZOMBIE_ELITE_DIGGER) {
				bounds.w += 10.0f;
			}
			const float centerY = bounds.y + bounds.h * 0.5f;
			const float currentCenterX = bounds.x + bounds.w * 0.5f;
			const float distance = std::hypot(
				currentCenterX - targetOrigin.x, centerY - targetOrigin.y);
			const float predictedCenterX = zombie->GetTargetLeadX(distance / kStarSpeed);
			const float predictedLeft = predictedCenterX - bounds.w * 0.5f;
			const float predictedRight = predictedLeft + bounds.w;

			// 竖直星只需目标的预测矩形跨过杨桃的 X；上下方向由目标所在行自然决定。
			if (predictedRight > targetOrigin.x && predictedLeft < targetOrigin.x) {
				found = true;
				return;
			}

			const float angleDegrees = std::atan2(
				centerY - targetOrigin.y, predictedCenterX - targetOrigin.x)
				* 180.0f / kPi;
			const int rowDistance = std::abs(row - mRow);
			if (rowDistance < 2) {
				found = (angleDegrees > 20.0f && angleDegrees < 40.0f)
					|| (angleDegrees < -25.0f && angleDegrees > -45.0f);
			}
			else {
				found = (angleDegrees > 25.0f && angleDegrees < 35.0f)
					|| (angleDegrees < -28.0f && angleDegrees > -38.0f);
			}
		});
	}
	return found;
}

void StarFruit::FireStarVolley()
{
	if (!mBoard || mIsPreview) return;

	AudioSystem::PlaySound(GameRandom::Chance()
		? ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT
		: ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);

	const float radians = kDiagonalDegrees * kPi / 180.0f;
	const float diagonalX = std::cos(radians) * kStarSpeed;
	const float diagonalY = std::sin(radians) * kStarSpeed;
	const std::array<Vector, 5> velocities = {
		Vector(-kStarSpeed, 0.0f),
		Vector(0.0f, kStarSpeed),
		Vector(0.0f, -kStarSpeed),
		Vector(diagonalX, diagonalY),
		Vector(diagonalX, -diagonalY),
	};
	const Vector launchPosition =
		GetPosition() + Vector(kBulletOffsetX, kBulletOffsetY);
	for (const Vector& velocity : velocities) {
		Bullet* star = mBoard->CreatePlantBullet(
			BulletType::BULLET_STAR, mRow, launchPosition, mPlantType);
		if (!star) continue;
		star->SetBulletDamage(kStarDamage);
		star->SetVelocityX(velocity.x);
		star->SetVelocityY(velocity.y);
	}
}

void StarFruit::SaveExtraData(nlohmann::json& j) const
{
	j["shootTimer"] = mShootTimer;
	j["shootInterval"] = mShootInterval;
}

void StarFruit::LoadExtraData(const nlohmann::json& j)
{
	mShootTimer = std::clamp(
		j.value("shootTimer", 0.0f), 0.0f, kRepeatShootIntervalMax);
	mShootInterval = std::clamp(
		j.value("shootInterval", kInitialShootInterval),
		kRepeatShootIntervalMin, kRepeatShootIntervalMax);
}

void StarFruit::SetShootCycleForTesting(
	float elapsedSeconds, float intervalSeconds)
{
	mShootInterval = std::clamp(
		intervalSeconds, kRepeatShootIntervalMin, kRepeatShootIntervalMax);
	mShootTimer = std::clamp(elapsedSeconds, 0.0f, mShootInterval);
}

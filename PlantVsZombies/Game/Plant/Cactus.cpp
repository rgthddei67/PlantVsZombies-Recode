#include "Cactus.h"

#include "../../ResourceKeys.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"

namespace
{
	constexpr float kShootIntervalSeconds = 1.5f;       // 两次射击动画之间的基础间隔，单位：秒
	constexpr float kTargetCheckIntervalSeconds = 0.6f; // 达到攻击间隔后重新扫描本行目标的节流时间，单位：秒
	constexpr float kOriginalShootFps = 35.0f;          // C# 仙人掌射击动画播放速率，单位：帧/秒
	constexpr float kReanimFps = 12.0f;                 // Cactus.reanim 的基础帧率，单位：帧/秒
	constexpr float kShootClipSpeed = kOriginalShootFps / kReanimFps; // 原版帧率折算为 Animator clip 倍率
	constexpr int kGroundShootEventFrame = 26;          // 主人给定的低姿态尖刺发射全局帧号
	const Vector kGroundSpikeOffset(30.0f, -27.0f);     // 原版低姿态发射点换算到当前格子中心的相对像素
}

void Cactus::SetupPlant()
{
	Plant::SetupPlant();
	if (mIsPreview || !mAnimator) return;

	mAnimator->AddFrameEvent(kGroundShootEventFrame, [this]() {
		ShootSpike();
	}, true);

	// TODO(气球僵尸): 正式空中状态契约落地后接入 anim_rise/anim_idlehigh/
	// anim_shootinghigh/anim_lower，并在主人给定的全局第 70 帧发射同一种 BULLET_SPIKE。
}

void Cactus::SaveExtraData(nlohmann::json& j) const
{
	j["shootTimer"] = mShootTimer;
}

void Cactus::LoadExtraData(const nlohmann::json& j)
{
	mShootTimer = j.value("shootTimer", 1.0f);
}

void Cactus::PlantUpdate()
{
	const float attackSpeed = GetAttackSpeedMultiplier();
	mShootTimer += DeltaTime::GetDeltaTime() * attackSpeed;
	if (mShootTimer < kShootIntervalSeconds || !HasZombieInRow()) return;

	mShootTimer = 0.0f;
	PlayTrackOnce("anim_shooting", "anim_idle",
		kShootClipSpeed * attackSpeed, 0.2f);
}

bool Cactus::HasZombieInRow()
{
	if (!mBoard) return false;

	mCheckZombieTimer += DeltaTime::GetDeltaTime();
	if (mCheckZombieTimer < kTargetCheckIntervalSeconds) return false;
	mCheckZombieTimer = 0.0f;

	const float cactusX = GetPosition().x;
	bool found = false;
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (found || !zombie || !zombie->IsActive()) return;
		const float zombieX = zombie->GetPosition().x;
		if (!zombie->IsMindControlled() && zombie->HasHead()
			&& zombieX >= cactusX && zombieX <= SCENE_WIDTH
			&& mBoard->CanPlantAcquireZombie(this, zombie)) {
			found = true;
		}
	});
	return found;
}

void Cactus::ShootSpike()
{
	if (!mBoard) return;

	AudioSystem::PlaySound(GameRandom::Chance()
		? ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT
		: ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
	mBoard->CreateBullet(BulletType::BULLET_SPIKE, mRow,
		GetVisualAnchorPosition() + kGroundSpikeOffset);
}

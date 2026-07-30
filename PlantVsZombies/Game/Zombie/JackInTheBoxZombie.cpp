#include "JackInTheBoxZombie.h"

#include "../AudioSystem.h"
#include "../Board.h"
#include "../Plant/Plant.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
	constexpr int kBodyHealth = 500;                    // C# 小丑僵尸本体生命值
	constexpr float kGroundRootMotionRate = 12.0f;     // _ground 根运动资源帧率换算基准
	constexpr float kAbilityAnimMultiplier = 1.8f;     // 全局动画倍率，使基类死亡轨约为原版 28 FPS
	constexpr float kReferenceVelocity = 0.67f;        // C# mVelX=0.66～0.68 的中值
	constexpr float kRunEffectiveClip = 2.25f;         // 约为普通僵尸 0.3 px/tick 的 2.23 倍
	constexpr float kEatEffectiveClip = 20.0f / 12.0f; // C# anim_eat 20 FPS 相对资源 12 FPS
	constexpr float kPopEffectiveClip = 28.0f / 12.0f; // C# anim_pop 28 FPS 相对资源 12 FPS
	constexpr int kPopTicksMin = 450;                  // C# 常规开盒随机倒计时下界，单位厘秒 tick
	constexpr int kPopTicksMax = 749;                  // C# 常规开盒随机倒计时上界，单位厘秒 tick
	constexpr int kFastPopRollMax = 19;                // 1/20 概率把开盒倒计时缩短为三分之一
	constexpr float kCSharpTicksPerSecond = 100.0f;    // C# phaseCounter 的厘秒换算
	constexpr float kLimpSpeedFactor = 2.0f;           // C# ZOMBIE_LIMP_SPEED_FACTOR
	constexpr float kSurpriseDelay = 0.3f;             // 开盒 30cs 后播放 surprise 音效
	constexpr int kExplosionDamage = 1800;             // C# ApplyBurn 的小丑爆炸伤害
	constexpr float kZombieBlastRadius = 115.0f;       // 原版桌面版小丑对僵尸圆形爆区半径，单位 px
	constexpr float kPlantBlastRadius = 90.0f;         // 原版桌面版小丑对植物圆形爆区半径，单位 px
	constexpr float kLoopVolume = 0.42f;               // 手摇盒循环声的独立音量
	constexpr float kOneShotVolume = 0.55f;            // 开盒、惊吓与爆炸一次性音效音量
	constexpr float kLimbVolume = 0.35f;               // 断肢断头音效音量

	bool CircleOverlapsRect(const Vector& center, float radius, const SDL_FRect& bounds)
	{
		const float nearestX = std::clamp(center.x, bounds.x, bounds.x + bounds.w);
		const float nearestY = std::clamp(center.y, bounds.y, bounds.y + bounds.h);
		const float dx = center.x - nearestX;
		const float dy = center.y - nearestY;
		return dx * dx + dy * dy <= radius * radius;
	}
}

int JackInTheBoxZombie::sLoopSoundUsers = 0;

JackInTheBoxZombie::~JackInTheBoxZombie()
{
	ReleaseLoopSound();
}

void JackInTheBoxZombie::SetupZombie()
{
	mBodyHealth = kBodyHealth;
	mBodyMaxHealth = kBodyHealth;
	mSpeed = kGroundRootMotionRate;
	mRunVelocity = mIsPreview
		? kReferenceVelocity
		: GameRandom::Range(0.66f, 0.68f);

	if (mIsPreview) {
		PlayTrack("anim_idle");
		return;
	}

	RegisterFrameEvents();
	const int countdownTicks = GameRandom::Range(kPopTicksMin, kPopTicksMax);
	mPopCountdown = static_cast<float>(countdownTicks)
		/ mRunVelocity * kLimpSpeedFactor / kCSharpTicksPerSecond;
	if (GameRandom::Range(0, kFastPopRollMax) == 0) {
		mPopCountdown /= 3.0f;
	}
	mPhase = Phase::RUNNING;
	PlayWalkAnimation(0.0f);
}

/** 注册主人确认的啃食、开盒爆炸与死亡消失帧。 */
void JackInTheBoxZombie::RegisterFrameEvents()
{
	mAnimator->AddFrameEvent(45, [this]() { EatTarget(); }, true);
	mAnimator->AddFrameEvent(66, [this]() { Explode(); });
	mAnimator->AddFrameEvent(89, [this]() { Die(); });
}

void JackInTheBoxZombie::Update()
{
	Zombie::Update();
	if (mIsPreview || mIsDead || mIsDying || mFrozenTimer > 0.0f) return;

	const float deltaTime = DeltaTime::GetDeltaTime();
	if (mPhase == Phase::RUNNING) {
		if (!mHasHead) return;
		mPopCountdown = std::max(0.0f, mPopCountdown - deltaTime);
		if (mPopCountdown <= 0.0f) BeginPop();
		return;
	}

	if (!mSurprisePlayed) {
		mSurpriseCountdown = std::max(0.0f, mSurpriseCountdown - deltaTime);
		if (mSurpriseCountdown <= 0.0f) PlaySurprise();
	}
}

void JackInTheBoxZombie::SetPopCountdownForTesting(float seconds)
{
	if (mPhase == Phase::RUNNING) {
		mPopCountdown = std::max(0.0f, seconds);
	}
}

void JackInTheBoxZombie::BeginPop()
{
	if (mPhase != Phase::RUNNING || !mHasHead || mIsDying || mIsDead) return;
	StopEatingForPop();
	mPhase = Phase::POPPING;
	mSurpriseCountdown = kSurpriseDelay;
	mSurprisePlayed = false;
	ReleaseLoopSound();
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BOING, kOneShotVolume);
	mAnimator->PlayTrackOnce("anim_pop", "",
		kPopEffectiveClip / kAbilityAnimMultiplier, 0.2f);
}

void JackInTheBoxZombie::PlaySurprise()
{
	if (mSurprisePlayed || mPhase != Phase::POPPING || mExplosionResolved) return;
	mSurprisePlayed = true;
	const std::string& sound = GameRandom::Range(0, 2) < 2
		? ResourceKeys::Sounds::SOUND_JACK_SURPRISE
		: ResourceKeys::Sounds::SOUND_JACK_SURPRISE2;
	AudioSystem::PlaySound(sound, kOneShotVolume);
}

void JackInTheBoxZombie::Explode()
{
	if (mExplosionResolved || mPhase != Phase::POPPING || mIsDead || !mBoard) return;
	mExplosionResolved = true;
	const Vector center = GetExplosionCenter();

	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_EXPLOSION, kOneShotVolume);
	if (g_particleSystem) g_particleSystem->EmitEffect("JackExplode", center);
	mBoard->ShakeBoard(4.0f, -6.0f);

	// 复制 ID 后再结算，避免 Charred/Die 的延迟销毁改变遍历来源。
	const std::vector<int> zombieIDs = mBoard->mEntityManager.GetAllZombieIDs();
	for (const int zombieID : zombieIDs) {
		Zombie* zombie = mBoard->mEntityManager.GetZombie(zombieID);
		if (!zombie || zombie == this || !zombie->IsActive()) continue;
		// 小丑爆炸只伤害敌对阵营；魅惑状态改变爆炸所属阵营而非扩大目标范围。
		if (zombie->IsMindControlled() == mIsMindControlled) continue;
		const ColliderComponent* collider = zombie->GetColliderComponent();
		if (!collider || !CircleOverlapsRect(center, kZombieBlastRadius,
			collider->GetBoundingBox())) {
			continue;
		}
		if (zombie->CanBeCharred() && zombie->mBodyHealth < kExplosionDamage) {
			zombie->Charred();
		}
		else {
			zombie->TakeDamage(kExplosionDamage, DamageSource::OTHER);
		}
	}

	if (!mIsMindControlled) {
		const std::vector<int> plantIDs = mBoard->mEntityManager.GetAllPlantIDs();
		for (const int plantID : plantIDs) {
			Plant* plant = mBoard->mEntityManager.GetPlant(plantID);
			if (!plant || !plant->IsActive()) continue;
			const ColliderComponent* collider = plant->GetColliderComponent();
			if (collider && CircleOverlapsRect(center, kPlantBlastRadius,
				collider->GetBoundingBox())) {
				plant->Die();
			}
		}
	}
	Die();
}

void JackInTheBoxZombie::StopEatingForPop()
{
	if (!mIsEating) return;
	if (mEatPlantID != NULL_PLANT_ID && mBoard) {
		if (Plant* plant = mBoard->mEntityManager.GetPlant(mEatPlantID);
			plant && plant->mEaterCount > 0) {
			--plant->mEaterCount;
		}
	}
	mIsEating = false;
	mEatPlantID = NULL_PLANT_ID;
	mEatZombieID = NULL_ZOMBIE_ID;
	OnStopEating();
}

void JackInTheBoxZombie::TakeDamage(
	int damage, DamageSource source, bool penetrateShield, bool discardShieldOverflow)
{
	if (mPhase == Phase::POPPING) return;
	Zombie::TakeDamage(damage, source, penetrateShield, discardShieldOverflow);
}

void JackInTheBoxZombie::StartEat(ColliderComponent* other)
{
	if (mPhase == Phase::POPPING) return;
	Zombie::StartEat(other);
}

void JackInTheBoxZombie::OnStartEating()
{
	PlayTrack("anim_eat", kEatEffectiveClip / kAbilityAnimMultiplier, 0.2f);
}

void JackInTheBoxZombie::ZombieMove(
	float scaledDelta, TransformComponent* transform)
{
	if (mPhase == Phase::POPPING) return;
	Zombie::ZombieMove(scaledDelta, transform);
}

void JackInTheBoxZombie::PlayWalkAnimation(float blendTime)
{
	if (mPhase == Phase::POPPING) return;
	const float speedRatio = mRunVelocity / kReferenceVelocity;
	PlayTrack("anim_walk",
		kRunEffectiveClip / kAbilityAnimMultiplier * speedRatio, blendTime);
}

void JackInTheBoxZombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	if (g_particleSystem) {
		// 原版小丑不覆写掉头贴图，沿用带完整下巴的普通僵尸头粒子。
		g_particleSystem->EmitEffect("ZombieHeadOff", GetPosition());
	}
	ReleaseLoopSound();
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LIMBS_POP, kLimbVolume);
}

void JackInTheBoxZombie::ArmDrop()
{
	if (!mHasArm) return;
	mAnimator->SetTrackImage("zombie_jackbox_outerarm_lower",
		ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_REANIM_ZOMBIE_JACKBOX_OUTERARM_LOWER2));
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieJackboxArmOff", GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LIMBS_POP, kLimbVolume);
}

void JackInTheBoxZombie::ZombieItemUpdate() const
{
	Zombie::ZombieItemUpdate();
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
	}
	if (!mHasArm) {
		mAnimator->SetTrackImage("zombie_jackbox_outerarm_lower",
			ResourceManager::GetInstance().GetTexture(
				ResourceKeys::Textures::IMAGE_REANIM_ZOMBIE_JACKBOX_OUTERARM_LOWER2));
	}
}

void JackInTheBoxZombie::PlaySpawnSound()
{
	ClaimLoopSound();
}

void JackInTheBoxZombie::ClaimLoopSound()
{
	if (mLoopSoundClaimed || mPhase != Phase::RUNNING || !mHasHead
		|| mIsPreview || mIsDying || mIsDead) {
		return;
	}
	mLoopSoundClaimed = true;
	++sLoopSoundUsers;
	AudioSystem::PlayLoopingSound(
		ResourceKeys::Sounds::SOUND_JACKINTHEBOX, kLoopVolume);
}

void JackInTheBoxZombie::ReleaseLoopSound()
{
	if (!mLoopSoundClaimed) return;
	mLoopSoundClaimed = false;
	sLoopSoundUsers = std::max(0, sLoopSoundUsers - 1);
	if (sLoopSoundUsers == 0) {
		AudioSystem::StopLoopingSound(ResourceKeys::Sounds::SOUND_JACKINTHEBOX);
	}
}

void JackInTheBoxZombie::Die()
{
	ReleaseLoopSound();
	Zombie::Die();
}

bool JackInTheBoxZombie::CanBeFrozen() const
{
	return mPhase != Phase::POPPING;
}

float JackInTheBoxZombie::GetAbilityAnimSpeedMultiplier() const
{
	return kAbilityAnimMultiplier;
}

Vector JackInTheBoxZombie::GetExplosionCenter() const
{
	if (const ColliderComponent* collider = GetColliderComponent()) {
		const SDL_FRect bounds = collider->GetBoundingBox();
		return Vector(bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f);
	}
	return GetPosition();
}

void JackInTheBoxZombie::SaveExtraData(nlohmann::json& j) const
{
	j["phase"] = static_cast<int>(mPhase);
	j["popCountdown"] = mPopCountdown;
	j["surpriseCountdown"] = mSurpriseCountdown;
	j["runVelocity"] = mRunVelocity;
	j["surprisePlayed"] = mSurprisePlayed;
	j["explosionResolved"] = mExplosionResolved;
}

void JackInTheBoxZombie::LoadExtraData(const nlohmann::json& j)
{
	const int phase = std::clamp(j.value("phase", 0), 0,
		static_cast<int>(Phase::POPPING));
	mPhase = static_cast<Phase>(phase);
	mPopCountdown = std::max(0.0f, j.value("popCountdown", 0.0f));
	mSurpriseCountdown = std::max(0.0f,
		j.value("surpriseCountdown", kSurpriseDelay));
	mRunVelocity = std::clamp(j.value("runVelocity", kReferenceVelocity),
		0.66f, 0.68f);
	mSurprisePlayed = j.value("surprisePlayed", false);
	mExplosionResolved = j.value("explosionResolved", false);
	if (mPhase == Phase::RUNNING && mHasHead && !mExplosionResolved) {
		ClaimLoopSound();
	}
}

#include "ZamboniZombie.h"

#include "../../GameRandom.h"
#include "../../ResourceKeys.h"
#include "../../ResourceManager.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../AudioSystem.h"
#include "../Board.h"
#include "../GameObjectManager.h"
#include "../ShadowComponent.h"
#include "../Plant/Plant.h"
#include "ZamboniCharred.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr int kZamboniHealth = 1350;                 // 原版冰车本体血量
	constexpr float kFastDriveSpeed = 28.0f;             // 出生区冰车最高前进速度，单位 px/s
	constexpr float kSlowDriveSpeed = 10.0f;              // 速度曲线在地图基准后 300px 处的理论下限，单位 px/s
	constexpr float kInnerDriveSpeed = 15.0f;            // 进入地图基准后 x<=400 区域的固定速度，单位 px/s
	constexpr float kDriveCurveLeftFromBaseX = 300.0f;   // 线性速度曲线左端相对当前地图坐标基准的 X
	constexpr float kDriveCurveRightFromBaseX = 700.0f;  // 线性速度曲线右端相对当前地图坐标基准的 X
	constexpr float kDriveCurveStopFromBaseX = 400.0f;   // 越过该相对位置后不再重算速度
	constexpr float kRoofDriveCoordinateBaseX = 0.0f;    // TODO：屋顶冰车解禁前按斜坡实测并独立填写，不能套用草坪基准
	constexpr float kColliderFromVisualX = -68.0f;       // 原版碰撞框左缘相对车辆稳定视觉原点的 X，单位 px
	constexpr float kColliderFromVisualY = 10.0f;        // 原版碰撞框上缘相对车辆稳定视觉原点的 Y，单位 px
	constexpr float kIceFrontFromVisualX = 50.0f;        // 冰道左端相对车辆稳定视觉原点的 X，单位 px
	constexpr float kAttackFromVisualX = -58.0f;         // 原版攻击框左缘相对车辆稳定视觉原点的 X，单位 px
	constexpr float kAttackFromVisualY = 10.0f;          // 原版攻击框上缘相对车辆稳定视觉原点的 Y，单位 px
	constexpr float kAttackWidth = 133.0f;                // 原版车辆攻击矩形宽度，单位 px
	constexpr float kAttackHeight = 140.0f;               // 原版车辆攻击矩形高度，单位 px
	constexpr float kRequiredPlantOverlap = 20.0f;        // 至少覆盖该水平像素数才判定碾压
	constexpr float kSmokeInterval = 0.25f;               // 二段损坏烟雾补发间隔，单位秒
	constexpr float kSmokeFromVisualX = -41.0f;           // C# 800×600 冒烟点换算到稳定视觉原点后的 X，单位 px
	constexpr float kSmokeFromVisualY = 95.0f;            // C# 800×600 冒烟点换算到稳定视觉原点后的 Y，单位 px
	constexpr int kCriticalHealth = 200;                  // 低于该血量时车辆持续抖动并自损
	constexpr float kCriticalShakeAmplitude = 0.35f;      // 低血量故障抖动幅度，单位 px；避免整车每帧 1px 跳变过强
	constexpr int kCriticalSelfDamage = 3;                // 原版高血量车辆每次自损值
	constexpr int kCriticalDamageRollMax = 4;             // 每次更新 1/5 概率自损
	constexpr float kDeathEffectFromVisualX = 12.0f;      // C# 爆炸点换算到车辆稳定视觉原点后的 X，单位 px
	constexpr float kDeathEffectFromVisualY = 73.0f;      // C# 换算值 83px 再按主人要求上移 10px 后的 Y
	constexpr float kCharredScale = 0.94f;                 // 冰车专属灰烬缩放
	constexpr float kCharredScaleAnchorOffsetY = 18.0f;   // 0.9 缩放后补回轮胎落地点，单位 px

	float HorizontalOverlap(const SDL_FRect& a, const SDL_FRect& b)
	{
		return std::max(0.0f,
			std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x));
	}
}

void ZamboniZombie::SetupZombie()
{
	mBodyMaxHealth = kZamboniHealth;
	mBodyHealth = kZamboniHealth;
	mNeedDropArm = false;
	mNeedDropHead = false;
	mHasArm = true;
	mHasHead = true;

	if (mAnimator) {
		SetAnimationSpeed(GameRandom::Range(0.7f, 0.9f));
		PlayTrack("anim_drive");
	}
	if (mCollider) {
		mCollider->size = Vector(153.0f, 140.0f);
		// C# 的车辆矩形相对其 +68/-23 绘制原点定义；换算到本项目的 mVisualOffset，
		// 保持车身、子弹碰撞与调试框重合，同时不让低血量画面抖动带着物理框漂移。
		mCollider->offset = mVisualOffset
			+ Vector(kColliderFromVisualX, kColliderFromVisualY);
		// 冰车不进入啃食状态；碾压由 ZombieUpdate 的原版攻击矩形统一结算。
		mCollider->onTriggerEnter = [this](ColliderComponent* other) { StartEat(other); };
		mCollider->onTriggerStay = [this](ColliderComponent* other) { StartEat(other); };
		mCollider->onTriggerExit = nullptr;
	}
	RemoveComponent<ShadowComponent>();
	mPoolShadow = nullptr;

	if (!mIsPreview) {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZAMBONI, 0.35f);
	}
}

void ZamboniZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (!transform || scaledDelta <= 0.0f) return;

	const float x = transform->GetPosition().x;
	const float coordinateBaseX = GetDriveCoordinateBaseX();
	const float curveLeftX = coordinateBaseX + kDriveCurveLeftFromBaseX;
	const float curveRightX = coordinateBaseX + kDriveCurveRightFromBaseX;
	const float curveStopX = coordinateBaseX + kDriveCurveStopFromBaseX;
	if (x > curveStopX) {
		const float t = std::clamp(
			(x - curveLeftX) / (curveRightX - curveLeftX),
			0.0f, 1.0f);
		mDriveSpeed = kSlowDriveSpeed + (kFastDriveSpeed - kSlowDriveSpeed) * t;
	}
	else {
		mDriveSpeed = kInnerDriveSpeed;
	}

	float speed = mDriveSpeed;
	if (mBoard) {
		speed *= mBoard->GetZombieRainSpeedMultiplier();
		speed *= mBoard->GetZombieWindMoveMultiplier(false);
	}
	transform->Translate(-speed * scaledDelta, 0.0f);
}

float ZamboniZombie::GetDriveCoordinateBaseX() const
{
	if (mBoard && (mBoard->mBackGround == Background::ROOF
		|| mBoard->mBackGround == Background::NIGHT_ROOF)) {
		// 屋顶当前禁止生成冰车；先保留独立入口，未来必须按斜坡坐标实测后再解禁。
		return kRoofDriveCoordinateBaseX;
	}
	return CELL_INITALIZE_POS_X;
}

void ZamboniZombie::ZombieUpdate(float scaledTime)
{
	if (!mBoard || mIsPreview || mIsDead) return;

	const Vector position = GetPosition();
	const Vector stableVisualOrigin = position + mVisualOffset;
	mBoard->ExtendIceTrail(mRow, stableVisualOrigin.x + kIceFrontFromVisualX);
	CrushPlants();

	if (GetDamageStage() >= 2) {
		mSmokeTimer -= scaledTime;
		if (mSmokeTimer <= 0.0f) {
			mSmokeTimer += kSmokeInterval;
			if (g_particleSystem) {
				g_particleSystem->EmitEffect("ZamboniSmoke",
					stableVisualOrigin + Vector(kSmokeFromVisualX, kSmokeFromVisualY));
			}
		}
	}
	else {
		mSmokeTimer = 0.0f;
	}

	if (mBodyHealth < kCriticalHealth) {
		mDamageShakeOffset = Vector(
			GameRandom::Range(-kCriticalShakeAmplitude, kCriticalShakeAmplitude),
			GameRandom::Range(-kCriticalShakeAmplitude, kCriticalShakeAmplitude));
		if (GameRandom::Range(0, kCriticalDamageRollMax) == 0) {
			TakeBodyDamage(kCriticalSelfDamage);
		}
	}
	else {
		mDamageShakeOffset = Vector::zero();
	}
}

void ZamboniZombie::TakeBodyDamage(int damage)
{
	if (damage <= 0 || mIsDead) return;
	mBodyHealth = std::max(0, mBodyHealth - damage);
	ApplyDamageVisuals();
	if (mBodyHealth <= 0) {
		Die();
	}
}

int ZamboniZombie::GetDamageStage() const
{
	if (mBodyHealth <= mBodyMaxHealth / 3) return 2;
	if (mBodyHealth <= static_cast<int64_t>(mBodyMaxHealth) * 2 / 3) return 1;
	return 0;
}

void ZamboniZombie::ApplyDamageVisuals() const
{
	if (!mAnimator) return;
	const int stage = GetDamageStage();
	if (stage <= 0) return;

	auto& resources = ResourceManager::GetInstance();
	const char* suffix = stage == 1 ? "DAMAGE1" : "DAMAGE2";
	mAnimator->SetTrackImage("Zombie_zamboni_1", resources.GetTexture(
		std::string("IMAGE_ZOMBIE_ZAMBONI_1_") + suffix));
	mAnimator->SetTrackImage("Zombie_zamboni_2", resources.GetTexture(
		std::string("IMAGE_ZOMBIE_ZAMBONI_2_") + suffix));
}

void ZamboniZombie::ZombieItemUpdate() const
{
	ApplyDamageVisuals();
}

bool ZamboniZombie::CanCrushPlant(const Plant* plant) const
{
	if (!plant || plant->IsSquished() || plant->mRow != mRow) return false;
	switch (plant->mPlantType) {
	case PlantType::PLANT_CHERRYBOMB:
	case PlantType::PLANT_JALAPENO:
	case PlantType::PLANT_SQUASH:
		return false;
	case PlantType::PLANT_ICESHROOM:
	case PlantType::PLANT_DOOMSHROOM:
		return plant->GetSleepState();
	case PlantType::PLANT_SPIKEWEED:
	case PlantType::PLANT_SPIKEROCK:
		// TODO: 地刺草/地刺王实现后，在这里接入原版爆胎、轮胎粒子与延迟死亡流程。
		return false;
	default:
		return true;
	}
}

void ZamboniZombie::CrushPlants()
{
	const Vector stableVisualOrigin = GetPosition() + mVisualOffset;
	const SDL_FRect attackRect{
		stableVisualOrigin.x + kAttackFromVisualX,
		stableVisualOrigin.y + kAttackFromVisualY,
		kAttackWidth,
		kAttackHeight,
	};

	for (int id : mBoard->mEntityManager.GetAllPlantIDs()) {
		Plant* plant = mBoard->mEntityManager.GetPlant(id);
		if (!CanCrushPlant(plant)) continue;
		ColliderComponent* collider = plant->GetColliderComponent();
		if (!collider) continue;
		if (HorizontalOverlap(attackRect, collider->GetBoundingBox())
			>= kRequiredPlantOverlap) {
			plant->Squish();
		}
	}
}

void ZamboniZombie::Die()
{
	if (mIsDead) return;
	if (!mSuppressDeathEffects && !mIsPreview && !mDeathEffectsEmitted) {
		mDeathEffectsEmitted = true;
		if (g_particleSystem) {
			const Vector stableVisualOrigin = GetPosition() + mVisualOffset;
			g_particleSystem->EmitEffect("ZamboniExplosion",
				stableVisualOrigin
				+ Vector(kDeathEffectFromVisualX, kDeathEffectFromVisualY));
		}
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_EXPLOSION, 0.55f);
	}
	Zombie::Die();
}

void ZamboniZombie::Charred()
{
	if (mIsDead || !mBoard) return;
	GameObjectManager::GetInstance().CreateGameObjectImmediate<ZamboniCharred>(
		LAYER_GAME_ZOMBIE, ObjectType::OBJECT_ZOMBIE, mBoard,
		// 本项目的 gamedata 已把车辆逻辑原点换算成画面基点；专属灰烬直接继承最后一帧
		// 画面坐标，避免再次叠加 C# 旧坐标系的 +83/-26 而向右下偏移。
		GetVisualPosition() + Vector(0.0f, kCharredScaleAnchorOffsetY),
		AnimationType::ANIM_ZAMBONI_CHARRED,
		ColliderType::BOX, Vector::zero(), Vector::zero(), kCharredScale,
		"ZamboniCharred", true);
	mSuppressDeathEffects = true;
	Die();
}

void ZamboniZombie::StartEat(ColliderComponent* /*other*/)
{
	// 车辆永不切换到啃食状态；同排植物由 CrushPlants 走压扁契约。
}

Vector ZamboniZombie::GetVisualPosition() const
{
	return Zombie::GetVisualPosition() + mDamageShakeOffset;
}

void ZamboniZombie::SaveExtraData(nlohmann::json& j) const
{
	j["smokeTimer"] = mSmokeTimer;
}

void ZamboniZombie::LoadExtraData(const nlohmann::json& j)
{
	mSmokeTimer = std::clamp(j.value("smokeTimer", 0.0f), 0.0f, kSmokeInterval);
	ApplyDamageVisuals();
}

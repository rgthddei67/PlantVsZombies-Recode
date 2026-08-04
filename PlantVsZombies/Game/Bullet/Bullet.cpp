#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "Bullet.h"
#include "../GameObjectManager.h"
#include "../ObjectPool/BulletPool.h"
#include "../ShadowComponent.h"
#include "../AnimatedObject.h"
#include "../Cell.h"
#include "../../GameAPP.h"
#include "../../Logger.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {
	constexpr int kPeaDamage = 20;                    // 普通/寒冰/孢子基础伤害
	constexpr int kToxicPeaDamage = 15;               // 毒豆直击伤害；持续伤害由目标僵尸结算
	constexpr int kFireballDamage = 40;               // 火豌豆基础伤害，原版为普通豌豆两倍
	constexpr int kCabbageDamage = 40;                // 经典卷心菜直击伤害
	constexpr int kButterDamage = 40;                 // 经典黄油直击伤害；玉米粒沿用普通 20 点
	constexpr int kSpikeFrameDamage = 2;              // 仙人掌尖刺在 1x 下每个逻辑碰撞帧的基础伤害
	constexpr std::size_t kSpikePierceLimit = 4;       // 尖刺接触第四只不同僵尸后消失
	constexpr float kFireballSplashWidth = 100.0f;    // 火球命中后沿飞行方向的同排溅射宽度，单位：像素
	constexpr float kToxicFireballSplashWidth = 30.0f; // 毒素火球以更窄范围换取溅射目标同样叠毒，单位：像素
	constexpr int kSplashDamageDivisor = 3;           // 火球次要目标伤害为直击伤害的三分之一
	constexpr float kFireballForwardOffsetX = -25.0f; // FirePea.reanim 相对向右飞子弹逻辑原点的 X 偏移
	constexpr float kFireballBackwardOffsetX = 55.0f; // FirePea.reanim 相对向左飞子弹逻辑原点的 X 偏移
	constexpr float kFireballOffsetY = -25.0f;        // FirePea.reanim 相对子弹逻辑原点的 Y 偏移
	constexpr float kFireballImpactOffsetX = 38.0f;   // JalapenoFire 沿飞行方向相对子弹的 X 偏移绝对值
	constexpr float kFireballImpactOffsetY = -20.0f;  // 原版 JalapenoFire 命中特效相对子弹的 Y 偏移
	constexpr float kFireballImpactStartFrame = 3.0f; // Fire.reanim 约 25% 处起播，复刻原版 mAnimTime=0.25
	constexpr float kFireballImpactSpeed = 2.0f;      // Fire.reanim 为 12fps，原版命中特效按 24fps 播放
	constexpr float kFireballImpactScaleX = 0.7f;     // 原版火球命中特效横向缩放
	constexpr float kFireballImpactScaleY = 0.4f;     // 原版火球命中特效纵向缩放
	constexpr SDL_Color kToxicFireOverlay{165, 55, 235, 225}; // 毒豆点燃后的紫焰覆盖色，保留原动画明暗
	constexpr float kThreepeaterVerticalSpeed = 300.0f; // C# 在 100px 草地行高下每 10ms 移动 3px，换算为每秒像素
	constexpr float kThreepeaterDampingPerTick = 0.97f; // C# 每个 10ms 更新对纵向速度的衰减
	constexpr float kOriginalTickSeconds = 0.01f;       // 原版 Projectile 更新步长，单位：秒
	constexpr float kStarSpinSpeedMin = 286.0f;         // 原版 0.05rad/厘秒换算后的最小自旋，单位：度/秒
	constexpr float kStarSpinSpeedMax = 573.0f;         // 原版 0.10rad/厘秒换算后的最大自旋，单位：度/秒
	constexpr float kStarShadowExtraOffsetY = 15.0f;    // C# 星弹初始化时额外下移阴影 15px
	constexpr float kCabbageInitialRotation = -50.4f;   // C# -0.8796rad 的初始朝向，单位：度
	constexpr float kCabbageSpinSpeedMin = -458.4f;     // C# -0.08rad/厘秒换算后的自旋下限，单位：度/秒
	constexpr float kCabbageSpinSpeedMax = -114.6f;     // C# -0.02rad/厘秒换算后的自旋上限，单位：度/秒
	constexpr float kKernelInitialRotation = 0.0f;       // C# 玉米粒初始朝向，单位：度
	constexpr float kKernelSpinSpeedMin = -1145.9f;      // C# -0.20rad/厘秒换算后的自旋下限，单位：度/秒
	constexpr float kKernelSpinSpeedMax = -458.4f;       // C# -0.08rad/厘秒换算后的自旋上限，单位：度/秒
	constexpr float kKernelDrawScale = 0.95f;            // C# 玉米粒弹丸绘制缩放
	constexpr float kButterDrawScale = 0.8f;             // C# 黄油弹丸绘制缩放
	constexpr float kKernelImpactVolume = 0.3f;          // 玉米粒命中或落空 Foley 音量
	constexpr float kButterImpactVolume = 0.3f;          // 黄油命中或落空 Foley 音量
	constexpr float kLobCollisionArcHeight = 35.0f;     // 下降末段距基准轨迹不超过此高度时才允许碰撞，单位：px
	constexpr float kLobLandingGrace = 0.08f;           // 到达预测点后留给碰撞系统的命中宽限，单位：秒
	constexpr float kLobShadowHeightScale = 200.0f;     // 经典投掷物阴影随高度缩小公式的高度尺度，单位：px

	enum class BulletWindResponse {
		NONE,
		LIGHT_PROJECTILE
	};

	struct BulletWindProfile {
		BulletType type;
		BulletWindResponse response;
	};

	// 与 BulletType 枚举保持同序；新增或重排类型而未明确选择风力响应时编译直接失败。
	constexpr BulletWindProfile kBulletWindProfiles[] = {
		{ BulletType::BULLET_PEA,        BulletWindResponse::LIGHT_PROJECTILE },
		{ BulletType::BULLET_SNOWPEA,    BulletWindResponse::LIGHT_PROJECTILE },
		{ BulletType::BULLET_CABBAGE,    BulletWindResponse::NONE },
		{ BulletType::BULLET_MELON,      BulletWindResponse::NONE },
		{ BulletType::BULLET_PUFF,       BulletWindResponse::LIGHT_PROJECTILE },
		{ BulletType::BULLET_WINTERMELON, BulletWindResponse::NONE },
		{ BulletType::BULLET_FIREBALL,   BulletWindResponse::LIGHT_PROJECTILE },
		{ BulletType::BULLET_STAR,       BulletWindResponse::LIGHT_PROJECTILE },
		{ BulletType::BULLET_SPIKE,      BulletWindResponse::NONE },
		{ BulletType::BULLET_BASKETBALL, BulletWindResponse::NONE },
		{ BulletType::BULLET_KERNEL,     BulletWindResponse::NONE },
		{ BulletType::BULLET_COBBIG,     BulletWindResponse::NONE },
		{ BulletType::BULLET_BUTTER,     BulletWindResponse::NONE },
		{ BulletType::BULLET_ZOMBIE_PEA, BulletWindResponse::LIGHT_PROJECTILE },
		{ BulletType::BULLET_TOXICPEA,   BulletWindResponse::LIGHT_PROJECTILE },
		{ BulletType::BULLET_TOXICFIREBALL, BulletWindResponse::LIGHT_PROJECTILE },
	};

	constexpr bool BulletWindProfilesCoverEveryType()
	{
		constexpr std::size_t profileCount = sizeof(kBulletWindProfiles) / sizeof(kBulletWindProfiles[0]);
		if (profileCount != static_cast<std::size_t>(BulletType::NUM_BULLETS)) return false;
		for (std::size_t i = 0; i < profileCount; ++i) {
			if (kBulletWindProfiles[i].type != static_cast<BulletType>(i)) return false;
		}
		return true;
	}

	static_assert(BulletWindProfilesCoverEveryType(),
		"Every BulletType must explicitly select a typhoon wind response");

	BulletWindResponse WindResponseForBullet(BulletType type)
	{
		const int index = static_cast<int>(type);
		if (index < 0 || index >= static_cast<int>(BulletType::NUM_BULLETS)) {
			return BulletWindResponse::NONE;
		}
		return kBulletWindProfiles[index].response;
	}

	/** 返回对象池新建/复用时应恢复的类型基础伤害。 */
	int DefaultDamageForBullet(BulletType type)
	{
		if (type == BulletType::BULLET_FIREBALL
			|| type == BulletType::BULLET_TOXICFIREBALL) return kFireballDamage;
		if (type == BulletType::BULLET_CABBAGE) return kCabbageDamage;
		if (type == BulletType::BULLET_BUTTER) return kButterDamage;
		if (type == BulletType::BULLET_SPIKE) return kSpikeFrameDamage;
		if (type == BulletType::BULLET_TOXICPEA) return kToxicPeaDamage;
		return kPeaDamage;
	}

	/** 经典投手家族共用弹心绘制、解析阴影和上方绕盾语义。 */
	bool IsClassicLobbedBullet(BulletType type)
	{
		return type == BulletType::BULLET_CABBAGE
			|| type == BulletType::BULLET_KERNEL
			|| type == BulletType::BULLET_BUTTER;
	}

	/** 播放玉米粒落地或命中的两段随机 Foley。 */
	void PlayKernelImpactSound()
	{
		AudioSystem::PlaySound(GameRandom::Chance()
			? ResourceKeys::Sounds::SOUND_KERNELPULT
			: ResourceKeys::Sounds::SOUND_KERNELPULT2, kKernelImpactVolume);
	}

	/** 播放火球命中时的小段 JalapenoFire；依靠 PLAY_ONCE 完成态回收，不新增帧事件。 */
	class FireballImpact final : public AnimatedObject {
	public:
		FireballImpact(Board* board, const Vector& position, bool toxicFlame)
			: AnimatedObject(ObjectType::OBJECT_PARTICLE, board, position,
				AnimationType::ANIM_JALAPENO_FIRE, ColliderType::BOX,
				Vector::zero(), Vector::zero(), 1.0f, "FireballImpact", true),
			mToxicFlame(toxicFlame)
		{
		}

		void Start() override
		{
			GameObject::Start();
			if (!mAnimator) {
				GameObjectManager::GetInstance().DestroyGameObject(this);
				return;
			}

			mAnimator->SetFrameRangeToDefault();
			SetLoopType(PlayState::PLAY_ONCE);
			SetAnimationSpeed(kFireballImpactSpeed);
			SetCurrentFrame(kFireballImpactStartFrame);
			const Vector pivot = GetVisualPosition();
			mAnimator->SetRenderScale(
				kFireballImpactScaleX, kFireballImpactScaleY, pivot.x, pivot.y);
			if (mToxicFlame) {
				mAnimator->SetOverlayColor(kToxicFireOverlay.r, kToxicFireOverlay.g,
					kToxicFireOverlay.b, kToxicFireOverlay.a);
				mAnimator->EnableOverlayEffect(true);
			}
		}

	private:
		bool mToxicFlame = false;
	};
}

Bullet::Bullet(Board* board, BulletType bulletType, int row, const Vector& colliderRadius,
	const Vector& position) : GameObject(ObjectType::OBJECT_BULLET)
{
	this->mBoard = board;
	this->mBulletType = bulletType;
	this->mPoolType = bulletType;
	this->mDamage = DefaultDamageForBullet(bulletType);
	this->mRow = row;
	if (!mBoard) return;

	mTransform = AddComponent<TransformComponent>(position);
	mCollider = AddComponent<ColliderComponent>
		(colliderRadius, Vector(0, 0), ColliderType::CIRCLE);

	// C# Projectile.DrawShadow 明确让 Puff 直接返回；其余现有子弹使用豌豆阴影，
	// Snowpea 再按原版放大到 1.3 倍。实际提交由 BulletPool 的地面阴影阶段负责。
	if (mBulletType != BulletType::BULLET_PUFF) {
		mShadow = AddComponent<ShadowComponent>(ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_PLANTSHADOW));
		UpdateShadowLayout(position);
	}

	auto* collider = GetColliderComponent();
	collider->isTrigger = true;	// 设置为触发器
	collider->layerMask = CollisionLayer::BULLET;
	collider->collisionMask = CollisionLayer::ZOMBIE;
	collider->onTriggerEnter = [this](ColliderComponent* other) {
		HandleZombieContact(other);
	};
	if (mBulletType == BulletType::BULLET_SPIKE) {
		collider->onTriggerStay = [this](ColliderComponent* other) {
			HandleZombieContact(other);
		};
	}
}

void Bullet::Reset(Board* board, int row,
	const Vector& colliderRadius, const Vector& position) {
	mBoard = board;
	mBulletType = mPoolType;
	mRow = row;
	mHasHit = false;
	mCheckPositionTimer = 0.0f;
	mBulletID = NULL_BULLET_ID;
	mFromPool = true;  // 标记为来自对象池
	// 自定义伤害/速度只属于上一位对象池使用者；不复位会污染后续普通豌豆或孢子。
	mDamage = DefaultDamageForBullet(mPoolType);
	mVelocityX = 290.0f;
	mVelocityY = 0.0f;
	mRotationDegrees = 0.0f;
	mRotationSpeedDegrees = 0.0f;
	mThreepeaterMotion = false;
	mTargetsFlying = false;
	mLobbedMotion = false;
	mLobStart = Vector::zero();
	mLobTarget = Vector::zero();
	mLobElapsed = 0.0f;
	mLobDuration = 0.0f;
	mLobApexHeight = 0.0f;
	mHitTorchwoodColumn = -1;
	mPiercedZombieIDs.clear();
	mSpikeDamageRemainders.clear();
	mAnimatorAdvancedInParallel = false;
	ConfigurePresentation();

	// 重置 Transform
	if (mTransform) {
		mTransform->SetPosition(position);
	}
	UpdateShadowLayout(position);

	// 重置 Collider
	if (mCollider) {
		mCollider->mEnabled = true;
	}
}

void Bullet::Start()
{
	GameObject::Start();
	ConfigurePresentation();
}

void Bullet::Die()
{
	// 如果来自对象池，回收到池中
	if (mFromPool) {
		BulletPool* bulletPool = GameObjectManager::GetInstance().GetBulletPool();
		if (bulletPool) {
			bulletPool->Release(this);
			return;
		}
	}

	// 否则正常销毁
	GameObjectManager::GetInstance().DestroyGameObject(this);
}

void Bullet::Update()
{
	GameObject::Update();
	if (mProjectileAnimator) {
		if (mAnimatorAdvancedInParallel) {
			mAnimatorAdvancedInParallel = false;
		}
		else {
			mProjectileAnimator->Update();
		}
	}

	auto* transform = GetTransformComponent();
	float deltaTime = DeltaTime::GetDeltaTime();
	if (transform)
	{
		mCheckPositionTimer += deltaTime;
		if (mCheckPositionTimer >= 1.0f)
		{
			mCheckPositionTimer = 0.0f;
			Vector position = transform->GetPosition();
			if (position.x > static_cast<float>(SCENE_WIDTH + 20) ||
				position.x < -10.0f)
			{
				this->Die();
				return;
			}
		}
		if (mLobbedMotion) {
			if (!UpdateLobbedMotion(deltaTime)) return;
		}
		else {
			transform->Translate(
				GetWindAdjustedVelocityX() * deltaTime, mVelocityY * deltaTime);
		}
		const Vector position = transform->GetPosition();
		if (mRotationSpeedDegrees != 0.0f) {
			mRotationDegrees = std::fmod(
				mRotationDegrees + mRotationSpeedDegrees * deltaTime, 360.0f);
		}
		if (mBulletType == BulletType::BULLET_STAR) {
			if (position.y < 0.0f || position.y > static_cast<float>(SCENE_HEIGHT)) {
				Die();
				return;
			}
			UpdateStarRow(position);
		}
		UpdateShadowLayout(position);
		if (HitsRoofTerrain(position)) {
			HitRoofTerrain();
			return;
		}
		if (mThreepeaterMotion && !mLobbedMotion) {
			// 用指数折算保持不同固定步长下与 C# “每 10ms ×0.97”相同的弧线。
			mVelocityY *= std::pow(
				kThreepeaterDampingPerTick, deltaTime / kOriginalTickSeconds);
		}
	}
}

void Bullet::UpdateParallel(std::vector<DeferredEvent>& outBuf)
{
	if (!mProjectileAnimator) {
		mAnimatorAdvancedInParallel = false;
		return;
	}
	mProjectileAnimator->UpdateParallelDeferred(outBuf);
	mAnimatorAdvancedInParallel = true;
}

void Bullet::Draw(Graphics* g)
{
	if (mProjectileAnimator) {
		const Vector position = GetPosition();
		const float offsetX = mVelocityX < 0.0f
			? kFireballBackwardOffsetX : kFireballForwardOffsetX;
		mProjectileAnimator->Draw(
			g, position.x + offsetX, position.y + kFireballOffsetY, 1.0f);
		return;
	}

	if (mTexture) {
		Vector position = GetPosition();
		if (IsClassicLobbedBullet(mBulletType)) {
			position.x -= static_cast<float>(mTexture->width) * mScale * 0.5f;
			position.y -= static_cast<float>(mTexture->height) * mScale * 0.5f;
		}
		g->DrawTexture(mTexture, position.x, position.y,
			static_cast<float>(mTexture->width * mScale),
			static_cast<float>(mTexture->height) * mScale,
			mRotationDegrees);
	}
}

void Bullet::DrawShadow(Graphics* g)
{
	if (mShadow && mShadow->mEnabled) {
		mShadow->Draw(g);
	}
}

void Bullet::UpdateShadowLayout(const Vector& position)
{
	if (!mShadow) return;

	// 原分辨率 IMAGE_PEA_SHADOWS 是 42x9 的日/夜两格图，因此单格为 21x9；
	// C# Snowpea 分支把两轴统一放大 1.3 倍。
	constexpr float kPeaShadowWidth = 21.0f;
	constexpr float kPeaShadowHeight = 9.0f;
	float typeScale = 1.0f;
	if (mBulletType == BulletType::BULLET_SNOWPEA) {
		typeScale = 1.3f;
	}
	else if (mBulletType == BulletType::BULLET_FIREBALL
		|| mBulletType == BulletType::BULLET_TOXICFIREBALL) {
		typeScale = 1.4f;
	}
	else if (IsClassicLobbedBullet(mBulletType)) {
		const float height = mLobbedMotion
			? GetLobArcHeight()
			: std::max(0.0f, GetTerrainShadowY(position) - position.y);
		typeScale = std::clamp(
			kLobShadowHeightScale / (height + kLobShadowHeightScale),
			0.45f, 1.0f);
	}
	const float shadowWidth = kPeaShadowWidth * typeScale;
	const float shadowHeight = kPeaShadowHeight * typeScale;

	const Texture* shadowTexture = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_PLANTSHADOW);
	if (shadowTexture && shadowTexture->width > 0 && shadowTexture->height > 0) {
		mShadow->SetScale(Vector(
			shadowWidth / static_cast<float>(shadowTexture->width),
			shadowHeight / static_cast<float>(shadowTexture->height)));
	}

	// 主人校对：Y 与同一行豌豆射手的默认阴影中心一致，即格子中心 + 28。
	// X 偏移沿用 C#：Pea +3，Snowpea -1，Fireball/Spike 为 0。
	const float shadowLeftOffset = mBulletType == BulletType::BULLET_SNOWPEA
		? -1.0f
		: (mBulletType == BulletType::BULLET_STAR
			? 7.0f
			: ((mBulletType == BulletType::BULLET_FIREBALL
				|| mBulletType == BulletType::BULLET_TOXICFIREBALL
			|| mBulletType == BulletType::BULLET_SPIKE
			|| IsClassicLobbedBullet(mBulletType)) ? 0.0f : 3.0f));
	const float shadowOffsetY = GetTerrainShadowY(position) - position.y;
	mShadow->SetOffset(Vector(
		shadowLeftOffset + shadowWidth * 0.5f,
		shadowOffsetY));
}

float Bullet::GetTerrainShadowY(const Vector& position) const
{
	const float rowCenterY = mBoard
		? mBoard->GetRowCenterYAtX(mRow, position.x)
		: CELL_INITALIZE_POS_Y + static_cast<float>(mRow) * CELL_COLLIDER_SIZE_Y
			+ CELL_COLLIDER_SIZE_Y * 0.5f;
	return rowCenterY + 28.0f
		+ (mBulletType == BulletType::BULLET_STAR ? kStarShadowExtraOffsetY : 0.0f);
}

bool Bullet::HitsRoofTerrain(const Vector& position) const
{
	if (!mBoard || !mBoard->IsRoofBackground() || mRow < 0 || mRow >= mBoard->mRows) {
		return false;
	}

	float minimumClearance = 0.0f;
	switch (mBulletType) {
	case BulletType::BULLET_PEA:
	case BulletType::BULLET_SNOWPEA:
	case BulletType::BULLET_FIREBALL:
	case BulletType::BULLET_SPIKE:
	case BulletType::BULLET_TOXICPEA:
	case BulletType::BULLET_TOXICFIREBALL:
		minimumClearance = 28.0f;
		break;
	case BulletType::BULLET_PUFF:
		minimumClearance = 0.0f;
		break;
	case BulletType::BULLET_STAR:
		minimumClearance = 23.0f;
		break;
	default:
		// 卷心菜、玉米、篮球等拥有独立抛射高度，不能套用平射弹的屋顶阈值。
		return false;
	}
	return GetTerrainShadowY(position) - position.y < minimumClearance;
}

void Bullet::HitRoofTerrain()
{
	if (mHasHit) return;
	mHasHit = true;
	const Vector position = GetPosition();
	if (mBulletType == BulletType::BULLET_FIREBALL
		|| mBulletType == BulletType::BULLET_TOXICFIREBALL) {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IGNITE, 0.35f);
		const float impactOffsetX = mVelocityX < 0.0f
			? -kFireballImpactOffsetX : kFireballImpactOffsetX;
		GameObjectManager::GetInstance().CreateGameObject<FireballImpact>(
			LAYER_EFFECTS_WORLD, mBoard,
			position + Vector(impactOffsetX, kFireballImpactOffsetY),
			IsToxicFireball());
	}
	else if (g_particleSystem) {
		const char* effectName = nullptr;
		switch (mBulletType) {
		case BulletType::BULLET_SNOWPEA: effectName = "SnowPeaBulletHit"; break;
		case BulletType::BULLET_PUFF: effectName = "PuffShroomHit"; break;
		case BulletType::BULLET_TOXICPEA: effectName = "ToxicPeaBulletHit"; break;
		case BulletType::BULLET_PEA: effectName = "PeaBulletHit"; break;
		case BulletType::BULLET_STAR: effectName = "StarSplat"; break;
		case BulletType::BULLET_CABBAGE: effectName = "CabbageSplat"; break;
		case BulletType::BULLET_BUTTER: effectName = "ButterSplat"; break;
		default: break;
		}
		if (effectName) g_particleSystem->EmitEffect(effectName, position);
	}
	Die();
}

void Bullet::UpdateStarRow(const Vector& position)
{
	if (!mBoard || mBoard->mRows <= 0 || mVelocityY == 0.0f) return;

	// C# PixelToGridYKeepOnBoard 以弹丸左上点所在格为准；由当前首行中心和行高重建同一边界。
	const float rowHeight = mBoard->GetCellHeight();
	if (rowHeight <= 0.0f) return;
	const float boardTop =
		mBoard->GetRowCenterYAtX(0, position.x) - rowHeight * 0.5f;
	const int row = static_cast<int>(std::floor((position.y - boardTop) / rowHeight));
	mRow = std::clamp(row, 0, mBoard->mRows - 1);
}

void Bullet::EnableThreepeaterMotion(int sourceRow)
{
	if (sourceRow == mRow) return;
	mThreepeaterMotion = true;
	// 原版 300px/s 的衰减总位移约等于一格 100px；泳池行高为 85px，必须同比缩放，
	// 否则相邻行豌豆会越过目标行的视觉与碰撞基线。
	const float rowHeight = mBoard ? mBoard->GetCellHeight() : CELL_COLLIDER_SIZE_Y;
	const float verticalSpeed =
		kThreepeaterVerticalSpeed * rowHeight / CELL_COLLIDER_SIZE_Y;
	mVelocityY = mRow < sourceRow ? -verticalSpeed : verticalSpeed;
	if (mTransform) {
		UpdateShadowLayout(mTransform->GetPosition());
	}
}

void Bullet::BulletHitZombie(Zombie* zombie)
{
	if (!zombie) return;
	// 经典投掷物从上方砸向后层；普通二类护盾不承伤，加固防具由目标否决绕过。
	const bool requestsShieldBypass = IsClassicLobbedBullet(mBulletType);
	const bool bypassShield = zombie->ShouldProjectileBypassShield(
		mVelocityX, requestsShieldBypass);

	if (mBulletType == BulletType::BULLET_FIREBALL
		|| mBulletType == BulletType::BULLET_TOXICFIREBALL) {
		HitFireballZombie(zombie);
		return;
	}
	const bool canBeChilled =
		mBulletType == BulletType::BULLET_SNOWPEA && zombie->CanBeChilled();
	const bool playChillSound = canBeChilled
		&& zombie->GetCooldownTimer() <= 0.0f
		&& zombie->mHelmType == HelmType::HELMTYPE_NONE
		&& (zombie->mShieldType == ShieldType::SHIELDTYPE_NONE || bypassShield);

	if (mBulletType == BulletType::BULLET_KERNEL) {
		PlayKernelImpactSound();
	}
	else if (mBulletType == BulletType::BULLET_BUTTER) {
		AudioSystem::PlaySound(
			ResourceKeys::Sounds::SOUND_BUTTER, kButterImpactVolume);
		// 原版黄油声替代普通肉身 splat，但仍允许不可绕过防具或一类头盔发出材质声。
		PlayStandardImpactSound(zombie, bypassShield, false);
	}
	else {
		PlayStandardImpactSound(zombie, bypassShield);
	}
	// 风力先修正本发子弹的基础伤害，生存词条仍在 Zombie::TakeDamage 中统一且只缩放一次。
	zombie->TakeProjectileDamage(
		GetWindAdjustedDamage(), DamageSource::PLANT, mVelocityX,
		/*penetrateShield=*/false, /*discardShieldOverflow=*/false,
		requestsShieldBypass);
	// 直击死亡、魅惑或对象已回收时不得留下延迟伤害；具体门禁由目标集中维护。
	if (mBulletType == BulletType::BULLET_TOXICPEA) {
		zombie->ApplyToxinStack();
	}
	else if (mBulletType == BulletType::BULLET_BUTTER) {
		zombie->ApplyButter();
	}

	if (mBulletType == BulletType::BULLET_SNOWPEA) {
		if (canBeChilled) {
			if (playChillSound) {
				AudioSystem::PlaySound(
					ResourceKeys::Sounds::SOUND_COOLDOWNZOMBIE, 0.22f);
			}
			zombie->SetCooldown(7.5f, bypassShield);
		}
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("SnowPeaBulletHit", GetPosition());
		}
	}
	else if (mBulletType == BulletType::BULLET_PUFF) {
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("PuffShroomHit", GetPosition());
		}
	}
	else if (mBulletType == BulletType::BULLET_TOXICPEA) {
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("ToxicPeaBulletHit", GetPosition());
		}
	}
	else if (mBulletType == BulletType::BULLET_PEA) {
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("PeaBulletHit", GetPosition());
		}
	}
	else if (mBulletType == BulletType::BULLET_STAR) {
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("StarSplat", GetPosition());
		}
	}
	else if (mBulletType == BulletType::BULLET_CABBAGE) {
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("CabbageSplat", GetPosition());
		}
	}
	else if (mBulletType == BulletType::BULLET_BUTTER) {
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("ButterSplat", GetPosition());
		}
	}
}

void Bullet::ConfigurePresentation()
{
	mTexture = nullptr;
	mProjectileAnimator.reset();
	mAnimatorAdvancedInParallel = false;
	mScale = 0.9f;

	ResourceManager& resources = ResourceManager::GetInstance();
	switch (mBulletType) {
	case BulletType::BULLET_PEA:
		mTexture = resources.GetTexture(ResourceKeys::Textures::IMAGE_PROJECTILEPEA);
		break;
	case BulletType::BULLET_SNOWPEA:
		mTexture = resources.GetTexture(ResourceKeys::Textures::IMAGE_PROJECTILESNOWPEA);
		break;
	case BulletType::BULLET_TOXICPEA:
		mTexture = resources.GetTexture(ResourceKeys::Textures::IMAGE_PROJECTILETOXICPEA);
		break;
	case BulletType::BULLET_PUFF:
		mTexture = resources.GetTexture("IMAGE_PUFFSHROOM_PUFF1");
		mScale = 0.68f;
		break;
	case BulletType::BULLET_SPIKE:
		mTexture = resources.GetTexture(ResourceKeys::Textures::IMAGE_PROJECTILECACTUS);
		mScale = 1.0f;
		break;
	case BulletType::BULLET_STAR:
		mTexture = resources.GetTexture(ResourceKeys::Textures::IMAGE_PROJECTILE_STAR);
		mRotationSpeedDegrees = GameRandom::Range(
			kStarSpinSpeedMin, kStarSpinSpeedMax);
		if (GameRandom::Chance()) mRotationSpeedDegrees = -mRotationSpeedDegrees;
		break;
	case BulletType::BULLET_CABBAGE:
		mTexture = resources.GetTexture(
			ResourceKeys::Textures::IMAGE_REANIM_CABBAGEPULT_CABBAGE);
		mScale = 1.0f;
		mRotationDegrees = kCabbageInitialRotation;
		mRotationSpeedDegrees = GameRandom::Range(
			kCabbageSpinSpeedMin, kCabbageSpinSpeedMax);
		break;
	case BulletType::BULLET_KERNEL:
		mTexture = resources.GetTexture(
			ResourceKeys::Textures::IMAGE_CORNPULT_KERNAL);
		mScale = kKernelDrawScale;
		mRotationDegrees = kKernelInitialRotation;
		mRotationSpeedDegrees = GameRandom::Range(
			kKernelSpinSpeedMin, kKernelSpinSpeedMax);
		break;
	case BulletType::BULLET_BUTTER:
		mTexture = resources.GetTexture(
			ResourceKeys::Textures::IMAGE_CORNPULT_BUTTER);
		mScale = kButterDrawScale;
		mRotationDegrees = kCabbageInitialRotation;
		mRotationSpeedDegrees = GameRandom::Range(
			kCabbageSpinSpeedMin, kCabbageSpinSpeedMax);
		break;
	case BulletType::BULLET_FIREBALL:
	case BulletType::BULLET_TOXICFIREBALL: {
		auto reanim = resources.GetReanimation(
			resources.AnimationTypeToString(AnimationType::ANIM_FIREPEA));
		if (!reanim) {
			LOG_ERROR("Bullet") << "无法加载 FirePea.reanim";
			break;
		}
		mProjectileAnimator = std::make_shared<Animator>(reanim);
		mProjectileAnimator->SetFrameRangeToDefault();
		mProjectileAnimator->SetCurrentFrame(0.0f);
		mProjectileAnimator->SetSpeed(GameRandom::Range(50.0f, 80.0f) / 12.0f);
		mProjectileAnimator->SetFlipX(mVelocityX < 0.0f);
		if (IsToxicFireball()) {
			mProjectileAnimator->SetOverlayColor(kToxicFireOverlay.r, kToxicFireOverlay.g,
				kToxicFireOverlay.b, kToxicFireOverlay.a);
			mProjectileAnimator->EnableOverlayEffect(true);
		}
		mProjectileAnimator->Play(PlayState::PLAY_REPEAT);
		break;
	}
	default:
		break;
	}
}

void Bullet::ConfigureLobbedMotion(
	const Vector& target, float durationSeconds, float apexHeight)
{
	if (!mTransform) return;
	mLobbedMotion = true;
	mLobStart = mTransform->GetPosition();
	mLobTarget = target;
	mLobElapsed = 0.0f;
	mLobDuration = std::max(0.01f, durationSeconds);
	mLobApexHeight = std::max(0.0f, apexHeight);
	mVelocityX = (mLobTarget.x - mLobStart.x) / mLobDuration;
	mVelocityY = (mLobTarget.y - mLobStart.y) / mLobDuration
		- 4.0f * mLobApexHeight / mLobDuration;
	if (mCollider) mCollider->mEnabled = false;
	UpdateShadowLayout(mLobStart);
}

void Bullet::RestoreLobbedMotion(const Vector& start, const Vector& target,
	float elapsedSeconds, float durationSeconds, float apexHeight)
{
	mLobbedMotion = true;
	mLobStart = start;
	mLobTarget = target;
	mLobDuration = std::max(0.01f, durationSeconds);
	mLobElapsed = std::clamp(
		elapsedSeconds, 0.0f, mLobDuration + kLobLandingGrace);
	mLobApexHeight = std::max(0.0f, apexHeight);
	const float progress = GetLobProgress();
	const Vector position(
		mLobStart.x + (mLobTarget.x - mLobStart.x) * progress,
		mLobStart.y + (mLobTarget.y - mLobStart.y) * progress
			- GetLobArcHeight());
	if (mTransform) mTransform->SetPosition(position);
	mVelocityX = (mLobTarget.x - mLobStart.x) / mLobDuration;
	mVelocityY = (mLobTarget.y - mLobStart.y) / mLobDuration
		- (4.0f * mLobApexHeight / mLobDuration) * (1.0f - 2.0f * progress);
	if (mCollider) {
		mCollider->mEnabled = progress >= 0.5f
			&& GetLobArcHeight() <= kLobCollisionArcHeight;
	}
	UpdateShadowLayout(position);
}

float Bullet::GetLobProgress() const
{
	if (!mLobbedMotion || mLobDuration <= 0.0f) return 0.0f;
	return std::clamp(mLobElapsed / mLobDuration, 0.0f, 1.0f);
}

float Bullet::GetLobArcHeight() const
{
	const float progress = GetLobProgress();
	return 4.0f * mLobApexHeight * progress * (1.0f - progress);
}

bool Bullet::UpdateLobbedMotion(float deltaTime)
{
	if (!mTransform || mLobDuration <= 0.0f) return true;
	mLobElapsed += deltaTime;
	const float progress = GetLobProgress();
	const float arcHeight = GetLobArcHeight();
	const Vector position(
		mLobStart.x + (mLobTarget.x - mLobStart.x) * progress,
		mLobStart.y + (mLobTarget.y - mLobStart.y) * progress - arcHeight);
	mTransform->SetPosition(position);
	mVelocityX = (mLobTarget.x - mLobStart.x) / mLobDuration;
	mVelocityY = (mLobTarget.y - mLobStart.y) / mLobDuration
		- (4.0f * mLobApexHeight / mLobDuration) * (1.0f - 2.0f * progress);
	if (mCollider) {
		// 只在下降末段打开碰撞，飞越前排目标时不会在高空误触。
		mCollider->mEnabled = progress >= 0.5f
			&& arcHeight <= kLobCollisionArcHeight;
	}
	if (mLobElapsed >= mLobDuration + kLobLandingGrace) {
		HitLobbedGround();
		return false;
	}
	return true;
}

void Bullet::HitLobbedGround()
{
	if (mHasHit) return;
	mHasHit = true;
	if (mBulletType == BulletType::BULLET_KERNEL) {
		PlayKernelImpactSound();
	}
	else if (mBulletType == BulletType::BULLET_BUTTER) {
		AudioSystem::PlaySound(
			ResourceKeys::Sounds::SOUND_BUTTER, kButterImpactVolume);
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("ButterSplat", GetPosition());
		}
	}
	else if (g_particleSystem) {
		g_particleSystem->EmitEffect("CabbageSplat", GetPosition());
	}
	Die();
}

void Bullet::HandleZombieContact(ColliderComponent* other)
{
	if (!IsActive() || !other) return;
	auto* otherGameObject = other->GetGameObject();
	if (!otherGameObject || otherGameObject->GetObjectType() != ObjectType::OBJECT_ZOMBIE) {
		return;
	}

	auto* zombie = dynamic_cast<Zombie*>(otherGameObject);
	if (!zombie || zombie->mRow != mRow
		|| !zombie->CanBeTargetedByProjectile(mTargetsFlying)) return;
	const bool bypassShield = zombie->ShouldProjectileBypassShield(mVelocityX);

	if (mBulletType != BulletType::BULLET_SPIKE) {
		if (mHasHit) return;
		mHasHit = true;
		BulletHitZombie(zombie);
		Die();
		return;
	}

	const bool isNewZombie = std::find(
		mPiercedZombieIDs.begin(), mPiercedZombieIDs.end(), zombie->mZombieID)
		== mPiercedZombieIDs.end();
	if (isNewZombie) {
		mPiercedZombieIDs.push_back(zombie->mZombieID);
		mSpikeDamageRemainders.push_back(0.0f);
		// 帧伤本身不能每帧重播撞击声；每只不同目标只在首次接触时反馈一次。
		PlayStandardImpactSound(zombie, bypassShield);
	}

	const auto idIt = std::find(
		mPiercedZombieIDs.begin(), mPiercedZombieIDs.end(), zombie->mZombieID);
	const std::size_t targetIndex = static_cast<std::size_t>(
		std::distance(mPiercedZombieIDs.begin(), idIt));
	const bool reachedPierceLimit =
		isNewZombie && mPiercedZombieIDs.size() >= kSpikePierceLimit;
	const int frameDamage = zombie->ModifySpikeFrameDamage(mDamage, bypassShield);

	if (reachedPierceLimit) {
		// 达到穿透上限的目标没有后续 Stay 可消费额度，因此固定承受 1x 的完整帧伤后再回收。
		for (int i = 0; i < frameDamage && zombie->IsActive(); ++i) {
			zombie->TakeProjectileDamage(1, DamageSource::PLANT, mVelocityX);
		}
		mHasHit = true;
		Die();
		return;
	}

	// 固定逻辑步的回调次数不随倍速改变；用缩放逻辑时间累计额度，保证同样游戏时长
	// 在 0.5x/1x/2x 下按当前基础帧伤等比例推进，整数承伤总量保持一致。
	float& damageRemainder = mSpikeDamageRemainders[targetIndex];
	damageRemainder += static_cast<float>(frameDamage)
		* DeltaTime::GetDeltaTime() / DeltaTime::GetFixedStep();
	const int damageToApply = static_cast<int>(std::floor(damageRemainder + 1e-6f));
	if (damageToApply > 0) {
		damageRemainder -= static_cast<float>(damageToApply);
		// 每个整数额度仍是一次独立的 1 点帧伤；合并为 TakeDamage(N) 会让免伤次数、
		// 逐击取整和其他“每次受击”语义在 2x 下少触发。
		for (int i = 0; i < damageToApply && zombie->IsActive(); ++i) {
			zombie->TakeProjectileDamage(1, DamageSource::PLANT, mVelocityX);
		}
	}
}

void Bullet::SetVelocityX(float x)
{
	mVelocityX = x;
	if (mProjectileAnimator) {
		mProjectileAnimator->SetFlipX(mVelocityX < 0.0f);
	}
}

void Bullet::ConvertToFireball(int torchwoodColumn)
{
	if ((mBulletType != BulletType::BULLET_PEA
		&& mBulletType != BulletType::BULLET_TOXICPEA)
		|| mHitTorchwoodColumn == torchwoodColumn) {
		return;
	}

	mBulletType = mBulletType == BulletType::BULLET_TOXICPEA
		? BulletType::BULLET_TOXICFIREBALL
		: BulletType::BULLET_FIREBALL;
	mDamage = kFireballDamage;
	mHitTorchwoodColumn = torchwoodColumn;
	ConfigurePresentation();
	if (mTransform) {
		UpdateShadowLayout(mTransform->GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FIREPEA, 0.35f);
}

void Bullet::ConvertSnowPeaToPea(int torchwoodColumn)
{
	if (mBulletType != BulletType::BULLET_SNOWPEA
		|| mHitTorchwoodColumn == torchwoodColumn) {
		return;
	}

	mBulletType = BulletType::BULLET_PEA;
	mDamage = kPeaDamage;
	mHitTorchwoodColumn = torchwoodColumn;
	ConfigurePresentation();
	if (mTransform) {
		UpdateShadowLayout(mTransform->GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.2f);
}

void Bullet::PlayStandardImpactSound(
	const Zombie* zombie, bool bypassShield, bool includeBodySplat) const
{
	if (!zombie) return;

	if (zombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE
		|| zombie->mHelmType == HelmType::HELMTYPE_FOOTBALL) {
		AudioSystem::PlaySound(
			GameRandom::Range(1, 2) == 1
				? ResourceKeys::Sounds::SOUND_HITCONE
				: ResourceKeys::Sounds::SOUND_HITCONE2,
			0.2f);
		return;
	}

	if (zombie->mHelmType == HelmType::HELMTYPE_BUCKET
		|| (!bypassShield && zombie->mShieldType == ShieldType::SHIELDTYPE_DOOR)
		|| (!bypassShield && zombie->mShieldType == ShieldType::SHIELDTYPE_LADDER)
		|| zombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI
		|| zombie->mZombieType == ZombieType::ZOMBIE_GILDED_ZAMBONI) {
		AudioSystem::PlaySound(
			GameRandom::Range(1, 2) == 1
				? ResourceKeys::Sounds::SOUND_IRONHIT
				: ResourceKeys::Sounds::SOUND_IRONHIT2,
			0.2f);
		return;
	}
	if (!includeBodySplat) return;

	switch (GameRandom::Range(1, 3)) {
	case 1:
		AudioSystem::PlaySound(
			ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY1, 0.2f);
		break;
	case 2:
		AudioSystem::PlaySound(
			ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY2, 0.2f);
		break;
	default:
		AudioSystem::PlaySound(
			ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY3, 0.2f);
		break;
	}
}

void Bullet::RestoreSavedPresentationState(BulletType currentType, int hitTorchwoodColumn)
{
	mBulletType = currentType;
	mHitTorchwoodColumn = hitTorchwoodColumn;
	ConfigurePresentation();
	if (mTransform) {
		UpdateShadowLayout(mTransform->GetPosition());
	}
}

void Bullet::RestorePiercedZombieState(const std::vector<int>& zombieIDs,
	const std::vector<float>& damageRemainders)
{
	mPiercedZombieIDs.clear();
	mSpikeDamageRemainders.clear();
	if (mBulletType != BulletType::BULLET_SPIKE) return;

	for (std::size_t i = 0; i < zombieIDs.size(); ++i) {
		const int id = zombieIDs[i];
		if (std::find(mPiercedZombieIDs.begin(), mPiercedZombieIDs.end(), id)
			!= mPiercedZombieIDs.end()) {
			continue;
		}
		mPiercedZombieIDs.push_back(id);
		const float remainder = i < damageRemainders.size()
			? damageRemainders[i] : 0.0f;
		mSpikeDamageRemainders.push_back(std::clamp(remainder, 0.0f, 0.999999f));
		// 活跃子弹一旦达到穿透上限就已回收，因此合法存档至多包含上限减一只。
		if (mPiercedZombieIDs.size() + 1 >= kSpikePierceLimit) break;
	}
}

void Bullet::HitFireballZombie(Zombie* zombie)
{
	const int directDamage = GetWindAdjustedDamage();
	const bool bypassShield = zombie->ShouldProjectileBypassShield(mVelocityX);
	const bool toxicFireball = IsToxicFireball();
	if (zombie->IsFireResistant() && !bypassShield) {
		PlayStandardImpactSound(zombie, false);
		zombie->TakeProjectileDamage(directDamage, DamageSource::PLANT, mVelocityX);
		if (toxicFireball) zombie->ApplyToxinStack();
		return;
	}

	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IGNITE, 0.35f);
	if (!toxicFireball)
	{
		zombie->RemoveColdEffects();
	}

	std::vector<Zombie*> secondaryTargets;
	const float impactX = GetPosition().x;
	const float splashWidth = toxicFireball
		? kToxicFireballSplashWidth : kFireballSplashWidth;
	// 静止测试弹与普通向右火豆保持历史正向口径；反向火豆把同一宽度镜像到命中点左侧。
	const float splashLeft = mVelocityX < 0.0f
		? impactX - splashWidth : impactX;
	const float splashRight = mVelocityX < 0.0f
		? impactX : impactX + splashWidth;
	if (mBoard) {
		mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* candidate) {
			if (!candidate || candidate == zombie || !candidate->IsActive()
				|| candidate->IsDying() || candidate->IsMindControlled()
				|| candidate->IsFireResistant()) {
				return;
			}
			const ColliderComponent* collider = candidate->GetColliderComponent();
			if (!collider) return;
			const SDL_FRect bounds = collider->GetBoundingBox();
			if (bounds.x > splashRight || bounds.x + bounds.w < splashLeft) return;
			secondaryTargets.push_back(candidate);
			});
	}

	int splashDamage = std::max(1, directDamage / kSplashDamageDivisor);
	const int splashTotal = splashDamage * static_cast<int>(secondaryTargets.size());
	if (splashTotal > directDamage) {
		splashDamage = std::max(1,
			directDamage * directDamage
			/ (splashTotal * kSplashDamageDivisor));
	}

	// 原版 splash damage flag 会让二类护盾照常受损，同时把全额伤害继续传给本体。
	zombie->TakeProjectileDamage(
		directDamage, DamageSource::PLANT, mVelocityX, /*penetrateShield=*/true);
	// 紫焰豆以 30px 小范围限制铺毒规模；直击与实际受溅射的目标各叠一层。
	if (toxicFireball) zombie->ApplyToxinStack();
	for (Zombie* target : secondaryTargets) {
		if (target->IsActive() && !target->IsDying()) {
			target->TakeDamage(splashDamage, DamageSource::PLANT, true);
			if (toxicFireball) target->ApplyToxinStack();
		}
	}

	const float impactOffsetX = mVelocityX < 0.0f
		? -kFireballImpactOffsetX : kFireballImpactOffsetX;
	GameObjectManager::GetInstance().CreateGameObject<FireballImpact>(
		LAYER_EFFECTS_WORLD,
		mBoard,
		GetPosition() + Vector(impactOffsetX, kFireballImpactOffsetY),
		toxicFireball);
}

bool Bullet::IsTyphoonWindAffected() const
{
	return WindResponseForBullet(mBulletType) == BulletWindResponse::LIGHT_PROJECTILE;
}

float Bullet::GetWindAdjustedVelocityX() const
{
	if (!IsTyphoonWindAffected() || !mBoard || mVelocityX == 0.0f) return mVelocityX;
	return mVelocityX * mBoard->GetPlantBulletWindSpeedMultiplier(mVelocityX > 0.0f);
}

int Bullet::GetWindAdjustedDamage() const
{
	if (!IsTyphoonWindAffected() || !mBoard || mDamage <= 0 || mVelocityX == 0.0f) return mDamage;
	const float multiplier = mBoard->GetPlantBulletWindDamageMultiplier(mVelocityX > 0.0f);
	return std::max(1, static_cast<int>(std::lround(static_cast<float>(mDamage) * multiplier)));
}

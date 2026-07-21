#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "Bullet.h"
#include "../GameObjectManager.h"
#include "../ObjectPool/BulletPool.h"
#include "../ShadowComponent.h"
#include "../Cell.h"
#include "../../GameAPP.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
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
		{ BulletType::BULLET_STAR,       BulletWindResponse::NONE },
		{ BulletType::BULLET_SPIKE,      BulletWindResponse::NONE },
		{ BulletType::BULLET_BASKETBALL, BulletWindResponse::NONE },
		{ BulletType::BULLET_KERNEL,     BulletWindResponse::NONE },
		{ BulletType::BULLET_COBBIG,     BulletWindResponse::NONE },
		{ BulletType::BULLET_BUTTER,     BulletWindResponse::NONE },
		{ BulletType::BULLET_ZOMBIE_PEA, BulletWindResponse::NONE },
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
}

Bullet::Bullet(Board* board, BulletType bulletType, int row, const Vector& colliderRadius,
	const Vector& position) : GameObject(ObjectType::OBJECT_BULLET)
{
	this->mBoard = board;
	this->mBulletType = bulletType;
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
		auto* otherGameObject = other->GetGameObject();
		if (otherGameObject && otherGameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
			auto* zombie = dynamic_cast<Zombie*>(otherGameObject);
			if (zombie && zombie->mRow == this->mRow && !this->mHasHit) {
				this->mHasHit = true;
				this->BulletHitZombie(zombie);
				this->Die();
			}
		}
		};
}

void Bullet::Reset(Board* board, int row,
	const Vector& colliderRadius, const Vector& position) {
	mBoard = board;
	mRow = row;
	mHasHit = false;
	mCheckPositionTimer = 0.0f;
	mBulletID = NULL_BULLET_ID;
	mFromPool = true;  // 标记为来自对象池

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
			}
		}
		transform->Translate(GetWindAdjustedVelocityX() * deltaTime, mVelocityY * deltaTime);
	}
}

void Bullet::Draw(Graphics* g)
{
	if (mTexture) {
		Vector position = GetPosition();
		g->DrawTexture(mTexture, position.x, position.y,
			static_cast<float>(mTexture->width * mScale), static_cast<float>(mTexture->height) * mScale);
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
	const float typeScale = mBulletType == BulletType::BULLET_SNOWPEA ? 1.3f : 1.0f;
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
	// X 偏移沿用 C#：Pea +3，Snowpea -1。
	const float shadowLeftOffset = mBulletType == BulletType::BULLET_SNOWPEA ? -1.0f : 3.0f;
	const float shadowCenterY = CELL_INITALIZE_POS_Y
		+ static_cast<float>(mRow) * CELL_COLLIDER_SIZE_Y + CELL_COLLIDER_SIZE_Y * 0.5f + 28.0f;
	mShadow->SetOffset(Vector(
		shadowLeftOffset + shadowWidth * 0.5f,
		shadowCenterY - position.y));
}

void Bullet::BulletHitZombie(Zombie* zombie)
{
	// 风力先修正本发子弹的基础伤害，生存词条仍在 Zombie::TakeDamage 中统一且只缩放一次。
	zombie->TakeDamage(GetWindAdjustedDamage(), DamageSource::PLANT);
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

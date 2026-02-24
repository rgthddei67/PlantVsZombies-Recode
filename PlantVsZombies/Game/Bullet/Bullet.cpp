#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "Bullet.h"
#include "../GameObjectManager.h"
#include "../../GameApp.h"

Bullet::Bullet(Board* board, BulletType bulletType, int row, const GLTexture* texture, const Vector& colliderRadius,
	const Vector& position) : GameObject(ObjectType::OBJECT_BULLET)
{
	this->mBoard = board;
	this->mBulletType = bulletType;
	this->mTexture = texture;
	this->mRow = row;
	if (!mBoard) return;

	mTransform = AddComponent<TransformComponent>(position);
	mCollider = AddComponent<ColliderComponent>
		(colliderRadius, Vector(0, 0), ColliderType::CIRCLE);
	auto collider = GetColliderComponent();
	collider->isTrigger = true;	// 设置为触发器
	collider->onTriggerEnter = [this](std::shared_ptr<ColliderComponent> other) {
		auto otherGameObject = other->GetGameObject();
		if (otherGameObject && otherGameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
			auto zombie = std::dynamic_pointer_cast<Zombie>(otherGameObject);
			if (zombie && zombie->mRow == this->mRow && !this->mHasHit) {
				this->mHasHit = true;
				zombie->TakeDamage(this->GetBulletDamage());
				this->BulletHitZombie(zombie);
				this->Die();
			}
		}
		};
}

void Bullet::Die()
{
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}

void Bullet::Update()
{
	auto transform = GetTransformComponent();
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
		transform->Translate(mVelocityX * deltaTime, mVelocityY * deltaTime);
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
#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "Bullet.h"
#include "../GameObjectManager.h"
#include <SDL2/SDL_image.h>

Bullet::Bullet(Board* board, BulletType bulletType, int row, SDL_Texture* texture, const Vector& colliderRadius,
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
	collider->isTrigger = true;	// ÉèÖÃÎª´¥·¢Æ÷
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
		transform->Translate(mVelocityX * deltaTime, mVelocityY * deltaTime);
	}
}

void Bullet::Draw(SDL_Renderer* renderer)
{
	if (mTexture) {
		Vector position = GetPosition();
		int texWidth, texHeight;
		SDL_QueryTexture(mTexture, nullptr, nullptr, &texWidth, &texHeight);

		SDL_FRect destRect = {
			position.x,
			position.y,
			static_cast<float>(texWidth * mScale),
			static_cast<float>(texHeight * mScale)
		};

		SDL_RenderCopyF(renderer, mTexture, nullptr, &destRect);
	}
}
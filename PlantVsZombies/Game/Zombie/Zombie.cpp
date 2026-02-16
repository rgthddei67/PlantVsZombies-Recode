#include "Zombie.h"
#include "../Plant/Plant.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../GameObjectManager.h"

Zombie::Zombie(Board* board, ZombieType zombieType, float x, int row,
	AnimationType animType, const Vector& colliderSize, float scale, bool isPreview)
	: AnimatedObject(ObjectType::OBJECT_ZOMBIE, board,
		Vector(x, 0),
		animType,
		ColliderType::BOX,
		colliderSize,
		Vector(0, 0),
		scale,
		"Zombie",
		false) // 植物不自动销毁
{
	mBoard = board;
	mZombieType = zombieType;
	mRow = row;
	mIsPreview = isPreview;
}

void Zombie::SetupZombie()
{

}

void Zombie::Start()
{
	if (this->mIsPreview) {
		GameObject::Start();
		RemoveComponent<ColliderComponent>();
	}
	else {
		AnimatedObject::Start();
		auto shadowcomponent = AddComponent<ShadowComponent>
			(ResourceManager::GetInstance().GetTexture
			(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
		shadowcomponent->SetDrawOrder(-80);
	}
	this->PlayTrack("anim_hair");
	SetupZombie();
}

void Zombie::Update()
{
	AnimatedObject::Update();
	if (!mIsPreview) {
		ZombieUpdate();
	}
}

void Zombie::TakeDamage(int damage)
{

}

void Zombie::Die()
{
	StopAnimation();

	// 禁用碰撞体
	if (auto collider = mCollider.lock()) {
		collider->mEnabled = false;
	}

	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}
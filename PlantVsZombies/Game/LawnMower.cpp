#include "LawnMower.h"
#include "Board.h"
#include "Zombie/Zombie.h"
#include "GameObjectManager.h"
#include "../DeltaTime.h"
#include "../GameApp.h"

Mower::Mower(Board* board, MowerType type, AnimationType animType, float x, float y, int row, float scale)
	: AnimatedObject(ObjectType::OBJECT_LAWNMOWER, board,
		Vector(x, y),
		animType,
		ColliderType::BOX,
		Vector(60, 50),
		Vector(0, 0),
		scale,
		"LawnMower",
		false)
{
	this->mRow = row;
	this->mMowerType = type;

	auto collider = GetColliderComponent();
	if (!collider) return;

	collider->isTrigger = true;
	collider->onTriggerEnter = [this](std::shared_ptr<ColliderComponent> other) {
		auto go = other->GetGameObject();
		if (!go || go->GetObjectType() != ObjectType::OBJECT_ZOMBIE) return;

		// 首次碰撞触发移动
		if (mState == MowerState::IDLE) {
			Trigger();
		}

		Zombie* zombie = std::dynamic_pointer_cast<Zombie>(go).get();
		int totalHP = zombie->mBodyHealth + zombie->mHelmHealth + zombie->mShieldHealth;
		zombie->TakeDamage(totalHP);
		};
}

void Mower::Trigger()
{
	if (mMowerType == MowerType::WATER) {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_POOL_CLEANER, 0.4f);
	}
	else {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LAWNMOWER, 0.4f);
	}
	
	PlayTrack("anim_normal");

	mState = MowerState::MOVING;
	if (mBoard) {
		mBoard->SetRowLoseMower(mRow);
	}
}

void Mower::Update()
{
	AnimatedObject::Update();

	if (mState != MowerState::MOVING) return;

	float deltaTime = DeltaTime::GetDeltaTime();
	Vector pos = GetPosition();
	pos.x += mSpeed * deltaTime;
	SetPosition(pos);

	if (pos.x > static_cast<float>(SCENE_WIDTH) + 40.0f) {
		Die();
	}
}

void Mower::Die()
{
	if (auto collider = mCollider.lock()) {
		collider->mEnabled = false;
	}
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}

Vector Mower::GetPosition() const
{
	if (auto transform = mTransform.lock()) {
		return transform->GetPosition();
	}
	return Vector::zero();
}

void Mower::SetPosition(const Vector& position)
{
	if (auto transform = mTransform.lock()) {
		transform->SetPosition(position);
	}
}
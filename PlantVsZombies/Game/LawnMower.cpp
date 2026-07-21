#include "LawnMower.h"
#include "Board.h"
#include "Zombie/Zombie.h"
#include "GameObjectManager.h"
#include "ShadowComponent.h"
#include "../DeltaTime.h"
#include "../GameAPP.h"

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

	auto shadowcomponent = AddComponent<ShadowComponent>
		(ResourceManager::GetInstance().GetTexture
		(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
	shadowcomponent->SetDrawOrder(-80);
	shadowcomponent->SetOffset(Vector(40, 50));
	shadowcomponent->SetScale(Vector(1.0f, 1.0f));

	auto collider = GetColliderComponent();
	if (!collider) return;

	collider->isTrigger = true;
	collider->layerMask = CollisionLayer::MOWER;
	collider->collisionMask = CollisionLayer::ZOMBIE;
	collider->onTriggerStay = [this](ColliderComponent* other) {
		auto* go = other->GetGameObject();
		if (!go || go->GetObjectType() != ObjectType::OBJECT_ZOMBIE) return;
		auto* zombie = dynamic_cast<Zombie*>(go);
		if (!zombie) return;

		// 首次碰撞触发移动
		if (mState == MowerState::IDLE) {
			Trigger();
		}

		// 精英所在行保留正常启动和秒杀；其余行只失去小推车，不播放启动效果。
		if (zombie->ConsumesOtherMowersOnContact() && mBoard) {
			mBoard->RemoveOtherMowersWithoutTrigger(mMowerID);
		}

		zombie->TakeDamage(INT32_MAX, DamageSource::OTHER);

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
	if (mCollider) {
		mCollider->mEnabled = false;
	}
	GameObjectManager::GetInstance().DestroyGameObject(this);
}

Vector Mower::GetPosition() const
{
	if (mTransform) {
		return mTransform->GetPosition();
	}
	return Vector::zero();
}

void Mower::SetPosition(const Vector& position)
{
	if (mTransform) {
		mTransform->SetPosition(position);
	}
}

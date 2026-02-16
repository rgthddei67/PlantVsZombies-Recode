#include "Zombie.h"
#include "../Plant/Plant.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../GameObjectManager.h"
#include "../Plant/GameDataManager.h"

Zombie::Zombie(Board* board, ZombieType zombieType, float x, int row,
	AnimationType animType, float scale, bool isPreview)
	: AnimatedObject(ObjectType::OBJECT_ZOMBIE, board,
		Vector(x, 0),
		animType,
		ColliderType::BOX,
		Vector(50, 100),
		Vector(-25, -65),
		scale,
		"Zombie",
		false)
{
	mBoard = board;
	mZombieType = zombieType;
	mRow = row;
	mIsPreview = isPreview;
	if (!mBoard) return;

	// 逻辑同Plant.cpp
	SetPosition(Vector(x, board->RowToY(row)));
	mVisualOffset = GameDataManager::GetInstance().GetZombieOffset(zombieType);

	int random = GameRandom::Range(0, 1);

	GetAnimator()->SetTrackVisible("anim_tongue", static_cast<bool>(random));

	if (isPreview)
	{
		this->PlayTrack("anim_idle");
		return;
	}
	
	random = GameRandom::Range(0, 1);

	if (random == 0)
		this->PlayTrack("anim_walk");
	else
		this->PlayTrack("anim_walk2");

	auto collider = GetColliderComponent();
	collider->isTrigger = true;
	collider->onTriggerEnter = [this]
	(std::shared_ptr<ColliderComponent> other) {
		this->EatTarget(other);
		};
	collider->onTriggerExit = [this]
	(std::shared_ptr<ColliderComponent> other) {
		this->StopEat(other);
		};
	mSpeed += GameRandom::Range(-3, 3);
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
	SetAnimationSpeed(GameRandom::Range(1.1f, 1.4f));
	SetupZombie();
}

void Zombie::Update()
{
	AnimatedObject::Update();
	if (!mIsPreview) {
		float deltaTime = DeltaTime::GetDeltaTime();
		if (mEatPlantID != NULL_PLANT_ID)
		{
			mEatSoundTimer += deltaTime;
			if (mEatSoundTimer >= 0.7f)
			{
				mEatSoundTimer = 0;
				int random = GameRandom::Range(0, 1);
				if (random == 0)
				{
					AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT, 0.2f);
				}
				else
				{
					AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.2f);
				}
			}
			auto plant = mBoard->mEntityManager.GetPlant(mEatPlantID);
			plant->TakeDamage(1);
			if (plant->mPlantHealth <= 0)
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_FINISHEAT, 0.25f);
			}
		}
		if (mIsEating) return;
		Vector position = GetPosition();
		this->SetPosition(Vector(position.x -= (mSpeed * deltaTime), position.y));
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

Vector Zombie::GetVisualPosition() const {
	return GetTransformComponent()->GetWorldPosition() + mVisualOffset;
}

void Zombie::EatTarget(std::shared_ptr<ColliderComponent> other)
{
	if (mIsPreview)	return;
	auto gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject))
		{
			if (mEatPlantID != NULL_PLANT_ID || plant->mRow != this->mRow) return;	// 正在吃一个植物，那么不吃别的植物

			if (!mIsEating) {
				this->PlayTrack("anim_eat", 0.5f);
				this->SetOriginalSpeed(GetAnimationSpeed());
				this->SetAnimationSpeed(2.1f);
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
		}
	}
}

void Zombie::StopEat(std::shared_ptr<ColliderComponent> other)
{
	if (mIsPreview)	return;
	auto gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject))
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->PlayTrack("anim_walk2", 0.4f);
				this->SetAnimationSpeed(GetOriginalSpeed());
			}
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
		}
	}
}

Vector Zombie::GetPosition() const
{
	return GetTransformComponent()->GetWorldPosition();
}

void Zombie::SetPosition(const Vector& position)
{
	this->GetTransformComponent()->SetPosition(position);
}
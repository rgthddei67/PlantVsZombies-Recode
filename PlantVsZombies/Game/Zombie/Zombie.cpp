#include "Zombie.h"
#include "../Plant/Plant.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../GameObjectManager.h"
#include "../Plant/GameDataManager.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../GameApp.h"

Zombie::Zombie(Board* board, ZombieType zombieType, float x, float y, int row,
	AnimationType animType, float scale, bool isPreview)
	: AnimatedObject(ObjectType::OBJECT_ZOMBIE, board,
		Vector(x, y),
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

	mVisualOffset = GameDataManager::GetInstance().GetZombieOffset(zombieType);

	int random = GameRandom::Range(0, 1);

	mAnimator->SetTrackVisible("anim_tongue", static_cast<bool>(random));

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
	mAnimator->AddFrameEvent(216, [this]() { this->Die(); });
}

void Zombie::Start()
{
	AnimatedObject::Start();
	auto shadowcomponent = AddComponent<ShadowComponent>
		(ResourceManager::GetInstance().GetTexture
		(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
	shadowcomponent->SetDrawOrder(-80);
	if (this->mIsPreview) {
		RemoveComponent<ColliderComponent>();
	}
	SetAnimationSpeed(GameRandom::Range(1.1f, 1.4f));
	SetupZombie();
}

void Zombie::Update()
{
	AnimatedObject::Update();
	if (!mIsPreview) {
		float deltaTime = DeltaTime::GetDeltaTime();
		auto transform = this->GetTransformComponent();

		if (!transform) return;

		mCheckPositionTimer += deltaTime;
		if (mCheckPositionTimer >= 1.0f)
		{
			mCheckPositionTimer = 0.0f;
			Vector position = transform->GetPosition();
			if (position.x < 110.0f && mBoard)
			{
				mBoard->GameOver();
			}
			if (position.x > static_cast<float>(SCENE_WIDTH + 50) || position.x < -40.0f)
			{
				this->Die();
			}
		}

		if (!mHasHead)
		{
			mBodyHealth--;
			if (mBodyHealth <= 35)
			{
				if (!mIsDying)
				{
					PlayTrack("anim_death", 1.3f, 0.3f);
					mIsDying = true;
				}
				return;
			}
		}

		if (mEatPlantID != NULL_PLANT_ID && mHasHead)
		{
			mEatSoundTimer += deltaTime;
			if (mEatSoundTimer >= 0.7f)
			{
				mEatSoundTimer = 0;

				auto plant = mBoard->mEntityManager.GetPlant(mEatPlantID);
				plant->TakeDamage(mAttackDamage);
				if (plant->mPlantHealth <= 0)
				{
					AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_FINISHEAT, 0.25f);
				}

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
		}

		if (mIsEating) return;
		
		float speed = 0.0f;
		// 尝试从 _ground 轨道获取速度
		if (mAnimator) {
			speed = mAnimator->GetTrackVelocity("_ground") + mSpeed;
			if (mIsMindControlled)
			{
				transform->Translate(speed * deltaTime, 0);
			}
			else
			{
				transform->Translate(-speed * deltaTime, 0);
			}
		}
		ZombieUpdate();
	}
}

int Zombie::TakeShieldDamage(int damage)
{
	if (mShieldHealth <= 0)
	{
		mShieldType = ShieldType::SHIELDTYPE_NONE;
		return damage;
	}
	mShieldHealth -= damage;
	if (mShieldHealth <= 0)
	{
		int overflow = -mShieldHealth; // 剩余伤害
		mShieldHealth = 0;
		ShieldDrop();
		return overflow;
	}
	return 0;
}

int Zombie::TakeHelmDamage(int damage)
{
	if (mHelmHealth <= 0)
	{
		mHelmType = HelmType::HELMTYPE_NONE;
		return damage;
	}
	mHelmHealth -= damage;
	if (mHelmHealth <= 0)
	{
		int overflow = -mHelmHealth;
		mHelmHealth = 0;
		HelmDrop();
		return overflow;
	}
	return 0;
}

void Zombie::TakeBodyDamage(int damage)
{
	mBodyHealth -= damage;
	if (mBodyHealth < 0)
		mBodyHealth = 0;

	if (mBodyHealth <= mBodyMaxHealth * 2 / 3)
	{
		ArmDrop();
	}
	if (mBodyHealth <= mBodyMaxHealth / 3)
	{
		HeadDrop();
	}
}

void Zombie::TakeDamage(int damage)
{
	if (damage <= 0) return;

	SetGlowingTimer(0.1f);

	int remainingDamage = damage;

	// 1. 优先扣除二类
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
	{
		remainingDamage = TakeShieldDamage(remainingDamage);
	}

	// 2. 然后扣除头盔
	if (remainingDamage > 0 && mHelmType != HelmType::HELMTYPE_NONE)
	{
		remainingDamage = TakeHelmDamage(remainingDamage);
	}

	// 3. 最后扣除本体
	if (remainingDamage > 0)
	{
		TakeBodyDamage(remainingDamage);
	}
}

void Zombie::HeadDrop()
{
	if (!mHasHead) return;
	mHasHead = false;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_tongue", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect(ParticleType::ZOMBIE_HEAD_OFF,
		GetPosition() + Vector(-10, -50));
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void Zombie::ArmDrop()
{
	if (!mHasArm) return;
	mHasArm = false;
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
	g_particleSystem->EmitEffect(ParticleType::ZOMBIE_ARM_OFF,
		GetPosition() + Vector(-10, -8));
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void Zombie::ShieldDrop()
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE) return;
	mShieldType = ShieldType::SHIELDTYPE_NONE;
}

void Zombie::HelmDrop()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	mHelmType = HelmType::HELMTYPE_NONE;
}

void Zombie::Die()
{
	// 禁用碰撞体
	if (auto collider = mCollider.lock()) {
		collider->mEnabled = false;
	}
	this->mActive = false;
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}

Vector Zombie::GetVisualPosition() const {
	return GetTransformComponent()->GetPosition() + mVisualOffset;
}

void Zombie::EatTarget(std::shared_ptr<ColliderComponent> other)
{
	if (mIsPreview || mIsDying)	return;
	auto gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject))
		{
			if (mEatPlantID != NULL_PLANT_ID || plant->mRow != this->mRow) return;	// 正在吃一个植物，那么不吃别的植物

			if (!mIsEating) {
				this->PlayTrack("anim_eat", 2.1f, 0.3f);
				this->SetOriginalSpeed(GetAnimationSpeed());
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
		}
	}
}

void Zombie::StopEat(std::shared_ptr<ColliderComponent> other)
{
	if (mIsPreview || mIsDying)	return;
	auto gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject))
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->PlayTrack("anim_walk2", GetOriginalSpeed(), 0.3f);
			}
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
		}
	}
}

Vector Zombie::GetPosition() const
{
	return GetTransformComponent()->GetPosition();
}

void Zombie::SetPosition(const Vector& position)
{
	this->GetTransformComponent()->SetPosition(position);
}
#include "Zombie.h"
#include "ZombieCharred.h"
#include "../Plant/Plant.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../GameObjectManager.h"
#include "../Plant/GameDataManager.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../GameApp.h"

Zombie::Zombie(Board * board, ZombieType zombieType, float x, float y, int row,
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

	mHasTongue = static_cast<bool>(GameRandom::Range(0, 1));

	if (!mAnimator->GetTracksByName("anim_tongue").empty()) {
		mAnimator->SetTrackVisible("anim_tongue", mHasTongue);
	}

	if (isPreview)
	{
		this->PlayTrack("anim_idle");
		return;
	}

	if (GameRandom::Range(0, 1) == 0)
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

void Zombie::SetupZombieDeathEvent()
{
	mAnimator->AddFrameEvent(216, [this]() { this->Die(); });
}

void Zombie::ZombieItemUpdate() const
{
	if (!mHasArm) {
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
			GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
	}
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
		mAnimator->SetTrackVisible("anim_tongue", false);
		mAnimator->SetTrackVisible("anim_hair", false);
	}
	mAnimator->SetTrackVisible("anim_tongue", mHasTongue);
}

void Zombie::Charred()
{
	GameObjectManager::GetInstance().CreateGameObjectImmediate<ZombieCharred>
		(LAYER_GAME_ZOMBIE, ObjectType::OBJECT_ZOMBIE, mBoard, this->GetVisualPosition(), AnimationType::ANIM_ZOMBIE_CHARRED);
	Die();
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
	SetupZombieDeathEvent();
	SetupZombie();
}

void Zombie::CheckWin() const
{
	if (mBoard && mBoard->mCurrentWave >= mBoard->mMaxWave && mBoard->mZombieNumber <= 0)
	{
		mBoard->CreateTrophy(GetPosition());
	}
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
			if (position.x > static_cast<float>(SCENE_WIDTH + 50) || position.x < -20.0f)
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
					GetColliderComponent()->mEnabled = false;
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

				if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
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
	CheckShieldImage();
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
	CheckHelmImage();
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

	if (mNeedDropArm && mHasArm && mBodyHealth <= mBodyMaxHealth * 2 / 3)
	{
		ArmDrop();
		mHasArm = false;
	}
	if (mNeedDropHead && mHasHead && mBodyHealth <= mBodyMaxHealth / 3)
	{
		HeadDrop();
		mHasHead = false;
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
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_tongue", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect("ZombieHeadOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void Zombie::ArmDrop()
{
	if (!mHasArm) return;
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
	g_particleSystem->EmitEffect("ZombieArmOff",
		GetPosition());
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
	// 若死亡时仍在啃食植物，手动清理啃食状态（防止 mEaterCount 无法归零）
	if (mIsEating && mEatPlantID != NULL_PLANT_ID && mBoard) {
		if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
			plant->mEaterCount--;
		}
		mIsEating = false;
		mEatPlantID = NULL_PLANT_ID;
	}

	if (mBoard) {
		mBoard->mZombieNumber--;
		CheckWin();
	}
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
		if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject).get())
		{
			if (mEatPlantID != NULL_PLANT_ID || plant->mRow != this->mRow) return;	// 正在吃一个植物，那么不吃别的植物

			if (!mIsEating) {
				this->PlayTrack("anim_eat", 2.1f, 0.3f);
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
			plant->mEaterCount++;
		}
	}
}

void Zombie::StopEat(std::shared_ptr<ColliderComponent> other)
{
	if (mIsPreview || mIsDying)	return;
	auto gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject).get())
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->PlayTrack("anim_walk2", 0.0f, 0.3f);
				this->RestoreSpeed();
				plant->mEaterCount--;
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

void Zombie::ValidateEatingState(EntityManager& em)
{
	if (mIsEating && mEatPlantID != NULL_PLANT_ID) {
		auto plant = em.GetPlant(mEatPlantID);
		if (!plant) {
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			PlayTrack("anim_walk2", 0.0f, 0.3f);
			RestoreSpeed();
		}
		else {
			plant->mEaterCount++;
		}
	}
}
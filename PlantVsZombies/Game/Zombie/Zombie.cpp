#include "Zombie.h"
#include "ZombieCharred.h"
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

	mVisualOffset = GameDataManager::GetInstance().GetZombieOffset(zombieType);

	if (isPreview)
	{
		this->PlayTrack("anim_idle");
		return;
	}

	if (!mBoard) return;

	auto collider = GetColliderComponent();
	collider->isTrigger = true;
	collider->layerMask = CollisionLayer::ZOMBIE;
	collider->collisionMask = CollisionLayer::PLANT | CollisionLayer::BULLET | CollisionLayer::MOWER;
	collider->onTriggerEnter = [this](ColliderComponent* other) {
		this->StartEat(other);
		};
	collider->onTriggerStay = [this](ColliderComponent* other) {
		this->StartEat(other);
		};
	collider->onTriggerExit = [this](ColliderComponent* other) {
		this->StopEat(other);
		};

	mGroundTrackIndex = mAnimator->GetFirstTrackIndexByName("_ground");
}

void Zombie::SetupZombie()
{
	mAnimator->AddFrameEvent(216, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(152, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(171, [this]() { this->EatTarget(); }, true);

	mHasTongue = static_cast<bool>(GameRandom::Range(0, 1));

	if (!mAnimator->GetTracksByName("anim_tongue").empty()) {
		mAnimator->SetTrackVisible("anim_tongue", mHasTongue);
	}

	if (mIsPreview) return;

	mSpeed += GameRandom::Range(-3, 3);

	if (GameRandom::Range(0, 1) == 0)
		this->PlayTrack("anim_walk");
	else
		this->PlayTrack("anim_walk2");
}

void Zombie::SaveProtectedData(nlohmann::json& j) const {
	j["isMindControlled"] = mIsMindControlled;
	j["isEating"] = mIsEating;
	j["eatPlantID"] = mEatPlantID;
	j["hasHead"] = mHasHead;
	j["hasArm"] = mHasArm;
	j["hasTongue"] = mHasTongue;
	j["isDying"] = mIsDying;
	j["speed"] = mSpeed;
	j["cooldownTimer"] = mCooldownTimer;
	j["dyingTimer"] = mDyingTimer;
}

void Zombie::LoadProtectedData(const nlohmann::json& j) {
	mIsMindControlled = j.value("isMindControlled", false);
	mIsEating = j.value("isEating", false);
	mEatPlantID = j.value("eatPlantID", NULL_PLANT_ID);
	mHasHead = j.value("hasHead", true);
	mHasArm = j.value("hasArm", true);
	mHasTongue = j.value("hasTongue", false);
	mIsDying = j.value("isDying", false);
	mSpeed = j.value("speed", 10.0f);
	float cooldown = j.value("cooldownTimer", 0.0f);
	if (cooldown > 0.0f) {
		this->SetCooldown(cooldown);
	}

	mDyingTimer = j.value("dyingTimer", 0.0f);
	if (mDyingTimer >= 10.0f) {
		this->Die();
	}
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
		mCollider = nullptr;  // 缓存的裸指针随之失效，显式置空
	}
	SetAnimationSpeed(GameRandom::Range(1.1f, 1.4f));
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
		auto* transform = this->GetTransformComponent();

		if (!transform && !mBoard) return;

		if (mIsDying)
		{
			mDyingTimer += deltaTime;
			if (mDyingTimer >= 10.0f)
			{
				this->Die();
				return;
			}
		}

		// —— 减速滴答（用真实 deltaTime，保证以真实秒数倒计时） ——
		if (mCooldownTimer > 0.0f)
		{
			mCooldownTimer -= deltaTime;
			if (mCooldownTimer <= 0.0f)
			{
				mCooldownTimer = 0.0f;
				if (mAnimator)
				{
					mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);
					mAnimator->EnableOverlayEffect(false);
				}
			}
		}

		// —— 减速时 50% 缩放僵尸内部逻辑 deltaTime ——
		const float slowMul = (mCooldownTimer > 0.0f) ? 0.5f : 1.0f;
		const float scaledDelta = deltaTime * slowMul;

		mCheckPositionTimer += deltaTime;
		if (mCheckPositionTimer >= 1.0f)
		{
			mCheckPositionTimer = 0.0f;
			Vector position = transform->GetPosition();
			if (position.x < 110.0f && mBoard->mBoardState == BoardState::GAME)
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

		if (mIsEating) return;

		ZombieMove(scaledDelta, transform);
		ZombieUpdate(scaledDelta);
	}
}

void Zombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	float speed = 0.0f;
	// 尝试从 _ground 轨道获取速度
	if (mAnimator) {
		// GetTrackVelocity 内部已乘 animator mSpeed，减速时 SetSpeed(0.5) 已令其自动减半
		//  但 this->mSpeed 是独立的固定项，需单独在减速时乘 0.5 但是deltaTime又变小了，所以不用

		speed = (mGroundTrackIndex >= 0
			? mAnimator->GetTrackVelocity(mGroundTrackIndex)
			: mAnimator->GetTrackVelocity("_ground"))
			+ mSpeed;
		if (mIsMindControlled)
		{
			transform->Translate(speed * scaledDelta, 0);
		}
		else
		{
			transform->Translate(-speed * scaledDelta, 0);
		}
	}
}

void Zombie::SetCooldown(float timer)
{
	if (!mAnimator) return;

	// 首次进入减速：启用独立的 0.6x 速度倍率 + 蓝色 overlay。
	// 用 mExtraSpeedMultiplier 而非 SetSpeed，这样后续 PlayTrack / SetSpeed
	// （例如切换啃食、死亡动画）不会覆盖减速效果。
	if (mCooldownTimer <= 0.0f)
	{
		mAnimator->SetExtraSpeedMultiplier(mExtraSpeed * 0.6f);
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(80, 80, 255, 240);
	}

	// 已在减速中则取 max，避免短射缩短减速
	mCooldownTimer = std::max(mCooldownTimer, timer);
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
	if (mCollider) {
		mCollider->mEnabled = false;
	}
	this->mActive = false;
	GameObjectManager::GetInstance().DestroyGameObject(this);
}

Vector Zombie::GetVisualPosition() const {
	return GetTransformComponent()->GetPosition() + mVisualOffset;
}

void Zombie::EatTarget()
{
	if (mEatPlantID != NULL_PLANT_ID && mHasHead)
	{
		if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
			plant->TakeDamage(mAttackDamage);
			if (plant->mPlantHealth <= 0)
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_FINISHEAT, 0.15f);
			}

			int random = GameRandom::Range(0, 1);
			if (random == 0)
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT, 0.1f);
			}
			else
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.1f);
			}
		}
	}
}

void Zombie::StartEat(ColliderComponent* other)
{
	if (mIsPreview || mIsDying)	return;
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != NULL_PLANT_ID || plant->mRow != this->mRow) return;	// 正在吃一个植物，那么不吃别的植物

			if (!mIsEating) {
				this->PlayTrack("anim_eat", 2.1f, 0.2f);
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
			plant->mEaterCount++;
		}
	}
}

void Zombie::StopEat(ColliderComponent* other)
{
	if (mIsPreview || mIsDying)	return;
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->PlayTrack("anim_walk2", 0.0f, 0.2f);
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

void Zombie::Draw(Graphics* g)
{
	AnimatedObject::Draw(g);	// 先画本体动画

	if (!g || mIsPreview || !GameAPP::GetInstance().mShowZombieHP) return;

	// 直接用逻辑坐标：DrawText 与 Animator 的 DrawTextureMatrix 共享同一 projView，
	// Animator 画 sprite 时就是用裸逻辑坐标，文字必须同坐标系才能叠在对象上（勿转 World）
	Vector pos = GetPosition() + Vector(-40, -40);

	constexpr int   fontSize = 15;
	constexpr float lineHeight = 18.0f;	// 行距 ≈ 字号，逐行向下、无空行
	// 颜色是 0..255 范围（ToSDLColor 直接 static_cast，不乘 255），勿写成 0..1 否则全透明隐形
	const glm::vec4 lightBlue(150.0f, 200.0f, 255.0f, 255.0f);

	float y = pos.y;
	auto drawLine = [&](const std::string& text) {
		g->DrawText(text, ResourceKeys::Fonts::FONT_FZJZ, fontSize, lightBlue, pos.x, y);
		y += lineHeight;
		};

	// 本体（始终显示）
	drawLine(u8"本体: " + std::to_string(mBodyHealth) + u8"/" + std::to_string(mBodyMaxHealth));
	// 一类防具（有 mHelmType 才显示）
	if (mHelmType != HelmType::HELMTYPE_NONE)
		drawLine(u8"一类: " + std::to_string(mHelmHealth) + u8"/" + std::to_string(mHelmMaxHealth));
	// 二类防具（有 mShieldType 才显示）
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
		drawLine(u8"二类: " + std::to_string(mShieldHealth) + u8"/" + std::to_string(mShieldMaxHealth));
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
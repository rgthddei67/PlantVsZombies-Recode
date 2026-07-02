#include "Polevaulter.h"
#include "../ShadowComponent.h"
#include "../Plant/Plant.h"

void Polevaulter::SetupZombie()
{
	if (!mIsPreview) {
		mAnimator->AddFrameEvent(92, [this]() {
			this->EndJump();
			});
		mAnimator->AddFrameEvent(164, [this]() {
			this->Die();
			});
		mAnimator->AddFrameEvent(179, [this]() {
			this->EatTarget();
			}, true);
		mAnimator->AddFrameEvent(191, [this]() {
			this->EatTarget();
			}, true);

		this->SetAnimationSpeed(GameRandom::Range(2.2f, 3.2f));
		PlayTrack("anim_run");
		// 重写碰撞回调：RUNNING状态碰到植物触发跳跃，WALKING状态走基类吃植物逻辑
		auto collider = GetColliderComponent();
		if (collider) {
			collider->onTriggerEnter = [this](ColliderComponent* other) {
				if (mIsPreview || mIsDying) return;

				auto* gameObject = other->GetGameObject();
				if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
					// 持杆奔跑不停下啃（跳跃语义优先，径直跑过魅惑僵尸）；跳后 WALKING 才互啃
					if (mVaultState != VaultState::RUNNING) StartEat(other);
					return;
				}
				if (gameObject->GetObjectType() != ObjectType::OBJECT_PLANT) return;

				auto* plant = dynamic_cast<Plant*>(gameObject);
				if (!plant || plant->mRow != this->mRow) return;

				if (!mHasVaulted && mVaultState == VaultState::RUNNING) {
					StartJump();
				}
				else {
					// 跳跃后走基类吃植物逻辑
					StartEat(other);
				}
				};
		}
	}
	this->mSpeed = GameRandom::Range(15.0f, 18.0f);

	this->mBodyMaxHealth = 500;
	this->mBodyHealth = 500;

	if (auto shadowComponent = GetComponent<ShadowComponent>()) {
		shadowComponent->SetOffset(Vector(4, 42));
	}
}

void Polevaulter::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect("PolevaulterHeadOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void Polevaulter::ArmDrop()
{
	if (!mHasArm) return;
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_polevaulter_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_polevaulter_outerarm_upper", ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_POLEVAULTER_OUTERARM_UPPER2));
	g_particleSystem->EmitEffect("PolevaulterArmOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void Polevaulter::ZombieItemUpdate() const
{
	if (!mHasArm) {
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
		mAnimator->SetTrackVisible("Zombie_polevaulter_outerarm_lower", false);
		mAnimator->SetTrackImage("Zombie_polevaulter_outerarm_upper", ResourceManager::GetInstance().
			GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_POLEVAULTER_OUTERARM_UPPER2));
	}
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
		mAnimator->SetTrackVisible("anim_hair", false);
	}
}

void Polevaulter::StartJump()
{
	mVaultState = VaultState::JUMPING;

	// 播放跳跃动画
	PlayTrackOnce("anim_jump", "anim_walk", 2.3f);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_POLEVAULT_JUMP, 0.4f);

	// 跳跃期间禁用碰撞体，防止触发吃植物
	if (auto collider = GetColliderComponent()) {
		collider->mEnabled = false;
	}
	if (auto shadow = GetComponent<ShadowComponent>()) {
		shadow->mEnabled = false;
	}
}

void Polevaulter::EndJump()
{
	mVaultState = VaultState::WALKING;
	mHasVaulted = true;

	JumpMove(150.0f);

	// 切换为走路动画和普通速度：跳跃后永久降速，写入动画 base（而非临时 clip）
	SetAnimationSpeed(GameRandom::Range(0.9f, 1.7f));
	PlayTrack("anim_walk");
	mSpeed = GameRandom::Range(7.0f, 13.0f);

	// 恢复碰撞体
	if (auto collider = GetColliderComponent()) {
		collider->mEnabled = true;
	}
	if (auto shadow = GetComponent<ShadowComponent>()) {
		shadow->mEnabled = true;
	}
}

void Polevaulter::JumpMove(float distance)
{
	auto transform = GetTransformComponent();
	if (!transform) return;

	if (mIsMindControlled) {
		transform->Translate(distance, 0);
	}
	else {
		transform->Translate(-distance, 0);
	}
}

void Polevaulter::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (this->mVaultState == VaultState::JUMPING) return;
	Zombie::ZombieMove(scaledDelta, transform);
}

void Polevaulter::ValidateEatingState(EntityManager& em)
{
	if (mIsEating && mEatPlantID != NULL_PLANT_ID) {
		auto plant = em.GetPlant(mEatPlantID);
		if (!plant) {
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			PlayTrack("anim_walk", 0.0f, 0.2f);   // clip 清零，自动回落走速
		}
		else {
			plant->mEaterCount++;
		}
	}
	else if (mIsEating) {
		// mEatPlantID 为空却在啃：啃僵尸进行时存的档（mEatZombieID 不持久化）→ 回走路，碰撞下一帧重建互啃
		mIsEating = false;
		PlayTrack(WalkTrackAfterEat(), 0.0f, 0.2f);
	}
}

void Polevaulter::SaveExtraData(nlohmann::json& j) const
{
	j["vaultState"] = static_cast<int>(mVaultState);
	j["hasVaulted"] = mHasVaulted;
}

void Polevaulter::LoadExtraData(const nlohmann::json& j)
{
	mVaultState = static_cast<VaultState>(j.value("vaultState", 0));
	mHasVaulted = j.value("hasVaulted", false);

	// 如果正在啃食，不覆盖已恢复的啃食动画
	if (mIsEating) return;

	// 恢复对应状态的动画
	if (mVaultState == VaultState::WALKING) {
		PlayTrack("anim_walk");
	}
	else if (mVaultState == VaultState::JUMPING) {
		// 读档时如果正在跳跃，直接完成跳跃
		EndJump();
	}
}

void Polevaulter::StartEat(ColliderComponent* other)
{
	// 持杆奔跑不停下啃僵尸：onTriggerEnter 的 lambda 有 RUNNING 守卫，但基类 onTriggerStay
	// 也会调 StartEat（僵尸分支无跳跃状态概念），必须在 override 里同样拦住
	if (other->GetGameObject()->GetObjectType() == ObjectType::OBJECT_ZOMBIE &&
		mVaultState == VaultState::RUNNING) {
		return;
	}
	Zombie::StartEat(other);
}

void Polevaulter::StopEat(ColliderComponent* other)
{
	if (mIsPreview || mIsDying)	return;
	if (other->GetGameObject()->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
		Zombie::StopEat(other);
		return;
	}
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->PlayTrack("anim_walk", 0.0f, 0.2f);   // clip 清零，自动回落走速
				plant->mEaterCount--;
			}
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
		}
	}
}
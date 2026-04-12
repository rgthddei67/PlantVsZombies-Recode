#include "Polevaulter.h"
#include "../ShadowComponent.h"
#include "../Plant/Plant.h"

void Polevaulter::SetupZombie()
{
	if (!mIsPreview) {
		mAnimator->AddFrameEvent(92, [this]() {
			EndJump();
			});
		mAnimator->AddFrameEvent(164, [this]() {
			Die();
			});
		this->SetAnimationSpeed(GameRandom::Range(2.2f, 3.2f));
		PlayTrack("anim_run");
		// 重写碰撞回调：RUNNING状态碰到植物触发跳跃，WALKING状态走基类吃植物逻辑
		auto collider = GetColliderComponent();
		if (collider) {
			collider->onTriggerEnter = [this](std::shared_ptr<ColliderComponent> other) {
				if (mIsPreview || mIsDying) return;

				auto gameObject = other->GetGameObject();
				if (gameObject->GetObjectType() != ObjectType::OBJECT_PLANT) return;

				auto plant = std::dynamic_pointer_cast<Plant>(gameObject);
				if (!plant || plant->mRow != this->mRow) return;

				if (!mHasVaulted && mVaultState == VaultState::RUNNING) {
					StartJump();
				}
				else {
					// 跳跃后走基类吃植物逻辑
					EatTarget(other);
				}
				};
		}
	}
	this->mSpeed = GameRandom::Range(17.5f, 27.0f);

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

	// 切换为走路动画和普通速度
	PlayTrack("anim_walk", GameRandom::Range(0.9f, 1.7f));
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

void Polevaulter::ZombieMove(float deltaTime, TransformComponent* transform)
{
	if (this->mVaultState == VaultState::JUMPING) return;
	Zombie::ZombieMove(deltaTime, transform);
}

void Polevaulter::ValidateEatingState(EntityManager& em)
{
	if (mIsEating && mEatPlantID != NULL_PLANT_ID) {
		auto plant = em.GetPlant(mEatPlantID);
		if (!plant) {
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			PlayTrack("anim_walk", 0.0f, 0.3f);
			RestoreSpeed();
		}
		else {
			plant->mEaterCount++;
		}
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

void Polevaulter::StopEat(std::shared_ptr<ColliderComponent> other)
{
	if (mIsPreview || mIsDying)	return;
	auto gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject).get())
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->PlayTrack("anim_walk", 0.0f, 0.3f);
				this->RestoreSpeed();
				plant->mEaterCount--;
			}
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
		}
	}
}
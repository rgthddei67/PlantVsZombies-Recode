#include "Polevaulter.h"
#include "../ShadowComponent.h"
#include "../Plant/Plant.h"

#include <algorithm>

namespace {
	constexpr float kBakedVaultDistance = 150.0f;  // anim_jump 轨道内置的水平视觉位移，单位 px
}

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
					if (mVaultState == VaultState::WALKING) StartEat(other);
					return;
				}
				if (gameObject->GetObjectType() != ObjectType::OBJECT_PLANT) return;

				auto* plant = dynamic_cast<Plant*>(gameObject);
				if (!plant || plant->mRow != this->mRow) return;

				if (mVaultState == VaultState::RUNNING && !mHasVaulted) {
					StartJump();
				}
				else if (mVaultState == VaultState::WALKING) {
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
	mLastVaultDistance = 0.0f;
	mVaultExtraDistanceApplied = 0.0f;

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

	const float vaultDistance = GetVaultDistance();
	const float targetExtraDistance = vaultDistance - kBakedVaultDistance;
	const float remainingExtraDistance = targetExtraDistance - mVaultExtraDistanceApplied;

	// 动画自身已经把身体部件向前带了 150 px；这里只提交该根运动以及尚未消费的额外位移。
	JumpMove(kBakedVaultDistance + remainingExtraDistance);
	mVaultExtraDistanceApplied = targetExtraDistance;
	mLastVaultDistance = vaultDistance;

	// 切换为走路动画和普通速度：跳跃后永久降速，写入动画 base（而非临时 clip）
	SetAnimationSpeed(GameRandom::Range(0.9f, 1.7f));
	PlayWalkAnimation(0.0f);
	mSpeed = GameRandom::Range(7.0f, 13.0f);

	// 恢复碰撞体
	if (auto collider = GetColliderComponent()) {
		collider->mEnabled = true;
	}
	if (auto shadow = GetComponent<ShadowComponent>()) {
		shadow->mEnabled = true;
	}

	// 派生能力必须看到最终落点与已恢复的碰撞状态。
	OnVaultLanded();
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

float Polevaulter::GetVaultDistance() const
{
	return kBakedVaultDistance;
}

void Polevaulter::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (mVaultState == VaultState::JUMPING) {
		const auto [jumpBeginFrame, jumpEndFrame] = mAnimator->GetTrackRange("anim_jump");
		const float jumpFrameCount = static_cast<float>(jumpEndFrame - jumpBeginFrame);
		if (jumpFrameCount > 0.0f) {
			const float jumpProgress = std::clamp(
				(GetCurrentFrame() - static_cast<float>(jumpBeginFrame)) / jumpFrameCount,
				0.0f, 1.0f);
			const float targetExtraDistance =
				(GetVaultDistance() - kBakedVaultDistance) * jumpProgress;

			// 只把超出动画内置 150 px 的部分按实际动画进度逐帧补上，避免落地瞬移。
			JumpMove(targetExtraDistance - mVaultExtraDistanceApplied);
			mVaultExtraDistanceApplied = targetExtraDistance;
		}
		return;
	}
	Zombie::ZombieMove(scaledDelta, transform);
}

void Polevaulter::PlayWalkAnimation(float blendTime)
{
	if (mVaultState == VaultState::JUMPING) {
		// 入水切换只更新介质视觉；跳跃轨道承载落地帧事件，不能被稳态走路动画抢占。
		return;
	}

	PlayTrack("anim_walk", 0.0f, blendTime);
}

void Polevaulter::SaveExtraData(nlohmann::json& j) const
{
	j["vaultState"] = static_cast<int>(mVaultState);
	j["hasVaulted"] = mHasVaulted;
	j["vaultExtraDistanceApplied"] = mVaultExtraDistanceApplied;
}

void Polevaulter::LoadExtraData(const nlohmann::json& j)
{
	mVaultState = static_cast<VaultState>(j.value("vaultState", 0));
	mHasVaulted = j.value("hasVaulted", false);
	const float targetExtraDistance = GetVaultDistance() - kBakedVaultDistance;
	mVaultExtraDistanceApplied = std::clamp(
		j.value("vaultExtraDistanceApplied", 0.0f),
		std::min(0.0f, targetExtraDistance),
		std::max(0.0f, targetExtraDistance));

	// 如果正在啃食，不覆盖已恢复的啃食动画
	if (mIsEating) return;

	// 恢复对应状态的动画
	if (mVaultState == VaultState::WALKING) {
		PlayWalkAnimation(0.0f);
	}
	else if (mVaultState == VaultState::JUMPING) {
		// 读档时如果正在跳跃，直接完成跳跃
		EndJump();
	}
}

void Polevaulter::StartEat(ColliderComponent* other)
{
	// 碰撞对在本帧回调前已经收集完：组合植物的第二个回调即使看到碰撞体已关闭也仍会到达。
	// 状态机入口统一守卫，避免 RUNNING/JUMPING 被任何植物或僵尸碰撞旁路切成啃食动画。
	if (mVaultState != VaultState::WALKING) return;
	Zombie::StartEat(other);
}

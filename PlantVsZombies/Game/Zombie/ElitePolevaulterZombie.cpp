#include "ElitePolevaulterZombie.h"

#include "../AudioSystem.h"
#include "../Board.h"
#include "../../ParticleSystem/ParticleSystem.h"

namespace {
	constexpr int kElitePolevaulterHealth = 450;  // 精英撑杆本体基础血量
	constexpr float kEliteVaultDistance = 250.0f;  // 精英撑杆每次落地的逻辑推进距离，单位 px
	constexpr float kEliteAnimationSpeedMultiplier = 1.1f;  // 相对普通撑杆的统一动画速度倍率
}

/**
 * @brief 复用普通撑杆全部帧事件与状态机，只覆盖精英数值。
 */
void ElitePolevaulterZombie::SetupZombie()
{
	Polevaulter::SetupZombie();
	mBodyHealth = kElitePolevaulterHealth;
	mBodyMaxHealth = kElitePolevaulterHealth;
}

float ElitePolevaulterZombie::GetVaultDistance() const
{
	return kEliteVaultDistance;
}

float ElitePolevaulterZombie::GetAbilityAnimSpeedMultiplier() const
{
	return kEliteAnimationSpeedMultiplier;
}

/**
 * @brief 在精英最终落点生成一名独立的普通撑杆僵尸。
 */
void ElitePolevaulterZombie::OnVaultLanded()
{
	if (!mBoard || mIsPreview || mIsDying) return;

	const auto* transform = GetTransformComponent();
	if (!transform) return;

	mBoard->CreateZombie(ZombieType::ZOMBIE_POLEVAULTER, mRow, transform->GetPosition().x);
}

/**
 * @brief 隐藏普通撑杆头部轨道，并发射与绿色头带匹配的掉头粒子。
 */
void ElitePolevaulterZombie::HeadDrop()
{
	if (!mHasHead) return;

	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ElitePolevaulterHeadOff", GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

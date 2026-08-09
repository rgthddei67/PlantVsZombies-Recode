#include "RoofMarshalZombie.h"

#include "../AudioSystem.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../ResourceKeys.h"

/**
 * @brief 复用普通僵尸已有的走路、啃食和死亡帧事件，不注册新的动画事件。
 */
void RoofMarshalZombie::SetupZombie()
{
	Zombie::SetupZombie();
}

void RoofMarshalZombie::HeadDrop()
{
	if (!mHasHead || !mAnimator) return;

	// 先读取仍可见的头轨世界锚点，再同步隐藏头、下巴、舌头和军帽轨道。
	const Vector particlePosition = GetTrackWorldPosition("anim_head1");
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_tongue", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("RoofMarshalHeadOff", particlePosition);
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

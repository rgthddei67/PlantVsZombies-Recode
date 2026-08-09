#include "RoofMarshalZombie.h"

#include "../AudioSystem.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../ResourceKeys.h"
#include <algorithm>

namespace {
	constexpr int kBodyHealth = 12000;                    // 主人确认的首领本体生命值
	constexpr int kPlantAshDamageCap = 1800;              // 灰烬与土豆雷的单次基础伤害上限
	constexpr int kPlantInstantKillFallbackDamage = 1800; // 大嘴花直杀失败后结算的单次基础伤害
}

/**
 * @brief 复用普通僵尸已有的走路、啃食和死亡帧事件，不注册新的动画事件。
 */
void RoofMarshalZombie::SetupZombie()
{
	// 完整复用普通僵尸的帧事件和动作时序，只覆盖首领耐久。
	Zombie::SetupZombie();
	mBodyHealth = kBodyHealth;
	mBodyMaxHealth = kBodyHealth;
}

void RoofMarshalZombie::TakePlantAshDamage(int damage)
{
	// 土豆雷传入 INT32_MAX；在统一灰烬入口先压回常规爆炸伤害，再保留植物词条缩放。
	Zombie::TakePlantAshDamage(std::min(damage, kPlantAshDamageCap));
}

bool RoofMarshalZombie::TakePlantInstantKill()
{
	// 大嘴花不进入消化状态，但这次完整咬合仍通过正式植物伤害链造成伤害。
	TakeDamage(kPlantInstantKillFallbackDamage, DamageSource::PLANT);
	return false;
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

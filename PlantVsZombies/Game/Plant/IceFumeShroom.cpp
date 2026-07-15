#include "IceFumeShroom.h"
#include "../Board.h"

namespace {
	constexpr float kIceSlowSeconds = 2.5f;   // 命中减速时长；重复命中取 max 不叠加（Zombie::SetCooldown）
}

void IceFumeShroom::SetupPlant()
{
	mFumeDamage = 10;
	mFumeReach = 360.0f;   // 大喷菇 390 − 30
	FumeShroom::SetupPlant();

	// 蓝色覆盖 = 僵尸减速同色（Zombie::SetCooldown 的 80,80,255,240）。
	// 这是身份视觉，不搭减速状态的便车：预览/图鉴同样生效，故放在基类 mIsPreview 早退之外
	if (mAnimator) {
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(80, 80, 255, 240);
	}
}

void IceFumeShroom::OnFumeHit(Zombie* zombie)
{
	// 减速总闸 CanBeChilled（垂死/魅惑/预览豁免）；持盾（铁门/报纸）在 SetCooldown 内部 no-op——
	// 穿透伤害照吃、减速不吃。伤害已由基类 FumeAttack 结算，这里只挂状态
	if (zombie->CanBeChilled())
		zombie->SetCooldown(kIceSlowSeconds);
}

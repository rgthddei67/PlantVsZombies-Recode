#include "IceFumeShroom.h"
#include "../Board.h"

namespace {
	// 减速时长 2.0 < 攻击间隔 2.5：刻意留 0.3s 恢复空窗，
	// 否则圈内=永久 0.6 速、整行输出 ×1.67 超模（2026-07-15 主人裁定中档）。
	// 重复命中取 max 不叠加（Zombie::SetCooldown）。
	constexpr float kIceSlowSeconds = 2.2f;
}

void IceFumeShroom::SetupPlant()
{
	mFumeDamage = 10;
	mFumeReach = 360.0f;   // 大喷菇 390 − 30
	mShootTime = 2.5f;     // 大喷菇 1.5s；控制型放慢节奏，配合减速时长制造空窗
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

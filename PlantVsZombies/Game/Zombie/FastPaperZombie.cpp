#include "FastPaperZombie.h"

void FastPaperZombie::SetupZombie()
{
	// 复用读报僵尸的护盾类型、帧事件（131 死亡 / 85,206 啃食 / 144 狂怒换头+吼叫）与初始动画
	PaperZombie::SetupZombie();

	// 报纸贴图：reanim 默认贴的是 Zombie_paper_paper1，换成 FastZombie_paper_paper1（预览态也要换）
	mAnimator->SetTrackImage("Zombie_paper_paper", ResourceManager::GetInstance().
		GetTexture("IMAGE_FASTZOMBIE_PAPER_PAPER1"));

	// 加强版数值：更厚的报纸 + 更肉的本体（破碎阈值仍是 2/3、1/3，逻辑沿用基类 CheckShieldImage）
	this->mBodyHealth = 350;
	this->mBodyMaxHealth = 350;
	this->mShieldHealth = 700;
	this->mShieldMaxHealth = 700;

	if (mIsPreview) return;

	// 攻击更疼；狂暴前就略快——mExtraSpeed 乘在 EffectiveSpeed 最外层，腿部动画与地面位移一起缩放，不脱节。
	// 狂暴(ShieldDrop)倍率沿用基类：mSpeed*=4.5 / mAttackDamage*=2 / mExtraSpeed*=2.0（最终 extra≈2.5，比普通读报更快）。
	this->mAttackDamage = static_cast<int>(this->mAttackDamage * 1.5f);
	mExtraSpeed = 1.5f;
	mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);
}

void FastPaperZombie::CheckShieldImage()
{
	// 与 PaperZombie::CheckShieldImage 完全同构，只把报纸破碎贴图换成 FastZombie 版本。
	if (mShieldType == ShieldType::SHIELDTYPE_NONE) return;

	if (mShieldStage == ArmorBrokenState::NO_BROKEN && mShieldHealth <= static_cast<int64_t>(mShieldMaxHealth) * 2 / 3) {
		mShieldStage = ArmorBrokenState::A_LITTLE_BROKEN;
		mAnimator->SetTrackImage("Zombie_paper_paper", ResourceManager::GetInstance().
			GetTexture("IMAGE_FASTZOMBIE_PAPER_PAPER2"));
	}
	if (mShieldStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mShieldHealth <= mShieldMaxHealth / 3) {
		mShieldStage = ArmorBrokenState::REALLY_BROKEN;
		mAnimator->SetTrackImage("Zombie_paper_paper", ResourceManager::GetInstance().
			GetTexture("IMAGE_FASTZOMBIE_PAPER_PAPER3"));
	}
}

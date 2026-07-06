#include "DoorZombie.h"
#include "../Plant/Plant.h"

void DoorZombie::SetupZombie()
{
	this->ShowArm(false);

	if (mIsPreview) return;

	mAnimator->AddFrameEvent(216, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(152, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(171, [this]() { this->EatTarget(); }, true);

	this->mShieldHealth = 1100;
	this->mShieldMaxHealth = 1100;
	this->mShieldType = ShieldType::SHIELDTYPE_DOOR;

	mSpeed += GameRandom::Range(-3, 3);

	if (GameRandom::Range(0, 1) == 0)
		this->PlayTrack("anim_walk");
	else
		this->PlayTrack("anim_walk2");
}

void DoorZombie::OnStartEating()
{
	// 残端造型优先于啃食露臂（与 ZombieItemUpdate 的优先级一致）：穿透伤害可在门未掉时掉头
	// （HeadDrop→ShowBrokenArm，mHasArm 仍为 true），无头僵尸流血期间仍可能开吃，
	// 此处若 ShowArm(true) 会把 hand/lower 重新点亮——断臂复活。
	if (!mHasArm || !mHasHead) return;
	// 啃食露出常规手臂（门还在才露）。与 OnStopEating 对称——修复"啃敌方僵尸时无手臂"(52bd86d)。
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
		this->ShowArm(true);
}

void DoorZombie::OnStopEating()
{
	// 残端造型优先：断臂/掉头后停吃若 ShowArm(false) 会把残端（innerarm+upper）也藏掉，变成完全无臂。
	if (!mHasArm || !mHasHead) return;
	// 收尾把常规手臂藏回门后（门还在才藏）。与 OnStartEating 对称——修复"被魅惑后多一条手臂"(c0cb799)。
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
		this->ShowArm(false);
}

void DoorZombie::ShowArm(bool show) const
{
	if (!mAnimator) return;
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", show);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", show);
	mAnimator->SetTrackVisible("Zombie_outerarm_upper", show);
	mAnimator->SetTrackVisible("anim_innerarm1", show);
	mAnimator->SetTrackVisible("anim_innerarm2", show);
	mAnimator->SetTrackVisible("anim_innerarm3", show);
}

void DoorZombie::ShowBrokenArm() const
{
	if (!mAnimator) return;
	// 该亮的：内臂 + 外上臂；该灭的：外手掌 + 外下臂（断臂只剩上臂残端）。
	mAnimator->SetTrackVisible("anim_innerarm1", true);
	mAnimator->SetTrackVisible("anim_innerarm2", true);
	mAnimator->SetTrackVisible("anim_innerarm3", true);
	mAnimator->SetTrackVisible("Zombie_outerarm_upper", true);
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));

	// 断臂即抓不住门：把「持门手臂」一并隐藏。正常死亡时门先掉、ShieldDrop 已隐藏它们（此处无害 no-op）；
	// 但大喷菇穿透会在门未掉时打死本体（掉头 HeadDrop 走到这里），若不隐藏就残留一条完好的持门手臂。
	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor_hand", false);
}

void DoorZombie::CheckShieldImage()
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE) return;

	if (mShieldStage == ArmorBrokenState::NO_BROKEN && mShieldHealth <= static_cast<int64_t>(mShieldMaxHealth) * 2 / 3) {
		mShieldStage = ArmorBrokenState::A_LITTLE_BROKEN;

		mAnimator->SetTrackImage("anim_screendoor", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_SCREENDOOR2"));
	}
	if (mShieldStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mShieldHealth <= mShieldMaxHealth / 3) {
		mShieldStage = ArmorBrokenState::REALLY_BROKEN;

		mAnimator->SetTrackImage("anim_screendoor", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_SCREENDOOR3"));
	}
}

void DoorZombie::ShieldDrop()
{
	Zombie::ShieldDrop();
	mShieldStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("anim_screendoor", false);

	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_screendoor", false);
	mAnimator->SetTrackVisible("Zombie_innerarm_screendoor_hand", false);
	
	this->ShowArm(true);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieDoorOff",
			GetPosition());
	}
} 

void DoorZombie::TakeBodyDamage(int damage)
{
	mBodyHealth -= damage;
	if (mBodyHealth < 0)
		mBodyHealth = 0;

	if (mNeedDropArm && mHasArm && mShieldType == ShieldType::SHIELDTYPE_NONE 
		&& mBodyHealth <= static_cast<int64_t>(mBodyMaxHealth) * 2 / 3)
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

void DoorZombie::HeadDrop()
{
	if (!mHasHead) return;

	// 显示手臂，并且让它为断裂的造型
	if (mHasArm) {
		this->ShowBrokenArm();
	}

	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect("ZombieHeadOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}
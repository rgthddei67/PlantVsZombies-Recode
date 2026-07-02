#include "PaperZombie.h"
#include "../Plant/Plant.h"

namespace {
	// 狂暴（掉报纸）后奔跑轨道 anim_walk_nopaper 的 clip 速度。
	// EffectiveSpeed = clip * extra(狂暴后 extra≈1.1) → 越大腿越快、地面位移也越快（GetTrackVelocity 按 EffectiveSpeed 缩放）。
	constexpr float kNoPaperWalkClip = 4.5f;
	constexpr float kNoPaperEatClip = 3.5f;   // 狂暴啃食 anim_eat_nopaper 的 clip 速度（StartEat / 读档复用）
}

void PaperZombie::SetupZombie()
{
	this->mBodyHealth = 300;
	this->mBodyMaxHealth = 300;
	this->mShieldType = ShieldType::SHIELDTYPE_NEWSPAPER;
	this->mShieldHealth = 500;
	this->mShieldMaxHealth = 500;

	if (!mIsPreview) {
		PlayTrack("anim_walk");

		mAnimator->AddFrameEvent(131, [this]() {
			this->Die();
			});
		mAnimator->AddFrameEvent(85, [this]() {
			this->EatTarget();
			}, true);
		mAnimator->AddFrameEvent(206, [this]() {
			this->EatTarget();
			}, true);

		mAnimator->AddFrameEvent(144, [this]() {
			this->mIsGasp = false;
			this->mAnimator->SetTrackImage("anim_head1", ResourceManager::GetInstance().
				GetTexture("IMAGE_ZOMBIE_PAPER_MADHEAD"));

			if (GameRandom::Chance()) {
				AudioSystem::PlaySound("SOUND_NEWSPAPER_RARRGH", 0.3f);
			}
			else {
				AudioSystem::PlaySound("SOUND_NEWSPAPER_RARRGH2", 0.3f);
			}

			});
	}
	else {
		PlayTrack("anim_idle");
	}
}

void PaperZombie::CheckShieldImage()
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE) return;

	if (mShieldStage == ArmorBrokenState::NO_BROKEN && mShieldHealth <= static_cast<int64_t>(mShieldMaxHealth) * 2 / 3) {
		mShieldStage = ArmorBrokenState::A_LITTLE_BROKEN;
		mAnimator->SetTrackImage("Zombie_paper_paper", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_PAPER_PAPER2"));
	}
	if (mShieldStage == ArmorBrokenState::A_LITTLE_BROKEN &&
		mShieldHealth <= mShieldMaxHealth / 3) {
		mShieldStage = ArmorBrokenState::REALLY_BROKEN;
		mAnimator->SetTrackImage("Zombie_paper_paper", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_PAPER_PAPER3"));
	}
}

void PaperZombie::ShieldDrop()
{
	Zombie::ShieldDrop();
	mShieldStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("Zombie_paper_paper", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombiePaperOff",
			GetPosition());
	}

	AudioSystem::PlaySound("SOUND_NEWSPAPER_RIP", 0.3f);

	mHasNewspaper = false;
	mIsGasp = true;

	// 掉报纸后进入狂奔：位移固定项 ×1.8；奔跑快慢主要由 anim_walk_nopaper 的 clip 决定（逐轨道控制）。
	mSpeed *= 1.35f;
	mAttackDamage *= 2;

	// 再叠一层轻微全局倍率，乘在 EffectiveSpeed 最外层（与逐轨道 clip 正交，啃食/gasp/走路一并 ×1.1）。
	// 必须调用 SetExtraSpeedMultiplier 才会生效；带 cooldown 判断与减速逻辑保持一致。
	mExtraSpeed *= 1.4f;
	mAnimator->SetExtraSpeedMultiplier(mExtraSpeed * (mCooldownTimer > 0.0f ? 0.6f : 1.0f));

	// gasp 播完后的回切轨道：若狂暴前已在啃食则回到啃食，否则狂奔。
	// （修复"愤怒前已接触植物 → 愤怒后站着走路不吃"：StartEat 的 mEatPlantID 守卫会让它再也补不回啃食动画，
	//   所以这里直接把回切目标设成啃食。）第3参=gasp 速度(0→base)，第5参=回切轨道 clip。
	if (mIsEating)
		PlayTrackOnce("anim_gasp", "anim_eat_nopaper", 0.0f, 0.05f, kNoPaperEatClip);
	else
		PlayTrackOnce("anim_gasp", "anim_walk_nopaper", 0.0f, 0.05f, kNoPaperWalkClip);
}

void PaperZombie::LoadExtraData(const nlohmann::json& j)
{
	mShieldStage = static_cast<ArmorBrokenState>(
		j.value("shieldStage", static_cast<int>(ArmorBrokenState::NO_BROKEN)));
	mHasNewspaper = j.value("hasNewspaper", true);
	mIsGasp = j.value("isGasp", false);

	if (!mAnimator) return;

	// 走路/啃食动画的恢复由 GameInfoSaver 全局负责（animTrack+animFrame+animClipSpeed 原样恢复，
	// 再加一遍全局 ValidateEatingState 兜底“植物没了→走路”）。extra 速度层也已在基类 LoadProtectedData 恢复。
	// 这里只补两件全局兜不住的事：
	if (!mHasNewspaper) {
		// (1) 狂怒头图：144 帧换头事件读档后不会再触发，手动贴。
		mAnimator->SetTrackImage("anim_head1", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_PAPER_MADHEAD"));

		// (2) Bug1：存档卡在 gasp 时 animTrack==anim_gasp 会被当循环轨道恢复而永远卡死。
		//     仅此情形需要接管——跳过 gasp，按是否在啃食直接定格（粗糙处理）。
		if (mIsGasp) {
			mIsGasp = false;
			if (mIsEating)
				PlayTrack("anim_eat_nopaper", kNoPaperEatClip, 0.0f);
			else
				PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, 0.0f);
		}
	}
}

void PaperZombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head_pupils", false);
	mAnimator->SetTrackVisible("anim_head_look", false);
	mAnimator->SetTrackVisible("anim_hairpiece", false);
	mAnimator->SetTrackVisible("anim_head_jaw", false);
	mAnimator->SetTrackVisible("anim_head_glasses", false);
	mAnimator->SetTrackVisible("anim_hair", false);

	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieHeadOff",
			GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void PaperZombie::ArmDrop()
{
	if (!mHasArm) return;
	mAnimator->SetTrackVisible("Zombie_paper_hands", false);
	mAnimator->SetTrackVisible("Zombie_paper_leftarm_lower", false);
	mAnimator->SetTrackImage("Zombie_paper_leftarm_upper", ResourceManager::GetInstance().
		GetTexture("IMAGE_ZOMBIE_PAPER_LEFTARM_UPPER2"));

	if (g_particleSystem) {
		g_particleSystem->EmitEffect("PaperZombieArmOff",
			GetPosition());
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void PaperZombie::ValidateEatingState(EntityManager& em)
{
	if (mIsEating && mEatPlantID != NULL_PLANT_ID) {
		auto plant = em.GetPlant(mEatPlantID);
		if (!plant) {
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			if (mHasNewspaper)
				PlayTrack("anim_walk", 0.0f, 0.2f);
			else
				PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, 0.2f);
		}
		else {
			plant->mEaterCount++;
		}
	}
	else if (mIsEating) {
		// mEatPlantID 为空却在啃：啃僵尸进行时存的档（mEatZombieID 不持久化）→ 回走路，碰撞下一帧重建互啃
		mIsEating = false;
		if (mHasNewspaper)
			PlayTrack("anim_walk", 0.0f, 0.2f);
		else
			PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, 0.2f);
	}
}

void PaperZombie::StartEat(ColliderComponent* other)
{
	if (mIsPreview || mIsDying)	return;
	if (other->GetGameObject()->GetObjectType() == ObjectType::OBJECT_ZOMBIE) {
		Zombie::StartEat(other);
		return;
	}
	if (mIsGasp) return;   // 狂怒喘气期间不开吃，避免新触发的啃食动画盖掉 gasp
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != NULL_PLANT_ID || plant->mRow != this->mRow) return;	// 正在吃一个植物，那么不吃别的植物

			if (!mIsEating) {
				if (mHasNewspaper)
					this->PlayTrack("anim_eat", 2.1f, 0.2f);
				else
					this->PlayTrack("anim_eat_nopaper", kNoPaperEatClip, 0.2f);
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
			plant->mEaterCount++;
		}
	}
}

void PaperZombie::StopEat(ColliderComponent* other)
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
				if (mHasNewspaper)
					this->PlayTrack("anim_walk", 0.0f, 0.2f);
				else
					this->PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, 0.2f);
				plant->mEaterCount--;
			}
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
		}
	}
}

void PaperZombie::ResumeWalkAfterEat(float blendTime)
{
	// 基类默认会播 "anim_walk2"，而 PaperZombie.reanim 只有 anim_walk / anim_walk_nopaper。
	// 与 StopEat / ValidateEatingState 的选轨道逻辑保持一致：狂暴态带 clip 速度，普通态 clip 清零。
	if (mHasNewspaper)
		PlayTrack("anim_walk", 0.0f, blendTime);
	else
		PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, blendTime);
}

void PaperZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (!mIsGasp) {
		Zombie::ZombieMove(scaledDelta, transform);
	}
}
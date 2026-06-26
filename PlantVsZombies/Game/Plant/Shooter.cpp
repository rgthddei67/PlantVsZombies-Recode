#include "Shooter.h"
#include "GameDataManager.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"

void Shooter::SetupPlant() {
	Plant::SetupPlant();

	auto reanim = mAnimator->GetReanimation();
	if (!reanim) return;

	mAnimator->PlayTrack("anim_idle");

	// 1. 创建头部动画器
	mHeadAnim = std::make_shared<Animator>(reanim);
	mHeadAnim->SetSpeed(this->GetAnimationSpeed());   // 同步身体动画速度
	mHeadAnim->PlayTrack("anim_head_idle");
	mHeadAnim->SetLocalPosition(GameDataManager::GetInstance().
		GetPlantOffset(this->mPlantType));

	// 2. 将头部附加到身体轨道
	if (!mAnimator->GetTracksByName("anim_stem").empty()) {
		mAnimator->AttachAnimator("anim_stem", mHeadAnim);
	}

	mAnimator->SetSpeed(GameRandom::Range(1.1f, 1.3f));

	mHeadAnim->AddFrameEvent(64, [this]() {
		if (GameRandom::Chance())
		{
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.3f);
		}
		else {
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
		}
		this->ShootBullet();
		}, true);
}

void Shooter::PlantUpdate()
{
	// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0，自动 no-op）。
	float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;
	this->mShootTimer += (DeltaTime::GetDeltaTime() * mult);
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			// 动画同比例加快：吐弹的第 64 帧 frame event 跟上更短间隔
			mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 1.5f * mult, 0.2f);
		}
	}
}

bool Shooter::HasZombieInRow()
{
	if (mBoard)
	{
		mCheckZombieTimer += DeltaTime::GetDeltaTime();
		if (mCheckZombieTimer >= 0.6f)
		{
			mCheckZombieTimer = 0.0f;
			// 按行索引：只遍历本行僵尸，mRow 过滤已由桶保证。
			const float thisX = GetPosition().x;
			bool found = false;
			mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
				if (found) return;  // 已命中，跳过本行其余
				float zombieX = zombie->GetPosition().x;
				if (zombieX >= thisX && zombieX <= SCENE_WIDTH && zombie->HasHead())
					found = true;
			});
			return found;
		}
	}
	return false;
}
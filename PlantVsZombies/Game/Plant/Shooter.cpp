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
}

void Shooter::PlantUpdate()
{
	Plant::PlantUpdate();
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
            mShootTimer = 0;
            mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 1.5f, 0.2f);
            mHeadAnim->AddFrameEvent(63, [this]() {
                if (GameRandom::Chance())
                {
                    AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT, 0.3f);
                }
                else {
                    AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_SHOOTER_SHOOT2, 0.3f);
                }
                this->ShootBullet();
				});
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
            EntityManager& manager = mBoard->mEntityManager;
            std::vector<int> zombieIDs = manager.GetAllZombieIDs();
            for (auto zombieID : zombieIDs)
            {
                if (auto zombie = manager.GetZombie(zombieID))
                {
                    Vector zombiePositon = zombie->GetPosition();
                    if (zombie->mRow == this->mRow && zombiePositon.x <= SCENE_WIDTH &&
                        zombiePositon.x >= this->GetPosition().x)
                    {
                        return true;
                    }
                }
            }
        }
	}
	return false;
}
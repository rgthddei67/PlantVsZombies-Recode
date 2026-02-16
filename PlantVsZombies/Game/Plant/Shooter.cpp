#include "Shooter.h"
#include "GameDataManager.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"

void Shooter::SetupPlant() {
    Plant::SetupPlant();  // 基类初始化

    auto animator = GetAnimator();
    if (!animator) return;

    auto reanim = animator->GetReanimation();
    if (!reanim) return;

    animator->PlayTrack("anim_idle");

    // 1. 创建头部动画器（与身体使用同一 Reanimation 资源）
    mHeadAnim = std::make_shared<Animator>(reanim);
    mHeadAnim->SetSpeed(this->GetAnimationSpeed());   // 同步身体动画速度
    mHeadAnim->PlayTrack("anim_head_idle");
    mHeadAnim->SetLocalPosition(GameDataManager::GetInstance().
        GetPlantOffset(this->mPlantType));

    // 2. 将头部附加到身体轨道（优先使用 anim_stem，其次 anim_idle）
    if (!animator->GetTracksByName("anim_stem").empty()) {
        animator->AttachAnimator("anim_stem", mHeadAnim);
    }

    // 3. 可选：设置头部绘制顺序（如果 Animator 支持）
    // mHeadAnim->SetDrawOrder(mRenderOrder + 2);
}

void Shooter::PlantUpdate()
{
	Plant::PlantUpdate();
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
            mHeadAnim->PlayTrackOnce("anim_shooting", "anim_head_idle", 1.5f, 0.5f);
			mShootTimer = 0;
			ShootBullet();
		}
	}
}

bool Shooter::HasZombieInRow()
{
	if (mBoard)
	{
        EntityManager manager = mBoard->mEntityManager;
        std::vector<int> zombieIDs = manager.GetAllZombieIDs();
        for (auto& zombieID : zombieIDs)
        {
            if (auto zombie = manager.GetZombie(zombieID))
            {
                if (zombie->mRow == this->mRow &&
                    zombie->GetPosition().x >= this->GetPosition().x)
                {
                    return true;
                }
            }
        }
	}
	return false;
}
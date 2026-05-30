#include "PuffShroom.h"
#include "../ShadowComponent.h"

void PuffShroom::SetupPlant()
{
	auto shadow = GetComponent<ShadowComponent>();
	shadow->SetScale(Vector(0.6f, 0.6f));
	shadow->SetOffset(Vector(3, 30));

	Shroom::SetupPlant();

	if (mIsPreview) return;
	
	mAnimator->AddFrameEvent(28, [this]() {
		if (!mBoard) return;
		AudioSystem::PlaySound("SOUND_PUFF", 0.28f);
		Vector bulletPosition = GetPosition() + Vector(13, 13);
		mBoard->CreateBullet(BulletType::BULLET_PUFF, mRow, bulletPosition);
		}, true);
}

void PuffShroom::PlantUpdate()
{
	this->mShootTimer += DeltaTime::GetDeltaTime();
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			PlayTrackOnce("anim_shooting", "anim_idle", 1.5f, 0.2f);
		}
	}
}

bool PuffShroom::HasZombieInRow()
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
					float thisX = GetPosition().x;
					float zombieX = zombie->GetPosition().x;
					float dx = zombieX - thisX;

					if (zombie->mRow == this->mRow && dx >= 0 && dx <= 300.0f)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}
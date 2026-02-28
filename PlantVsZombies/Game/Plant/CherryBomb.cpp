#include "CherryBomb.h"

void CherryBomb::SetupPlant()
{
	if (mIsPreview) return;
	this->PlayTrack("anim_explode", 0.5f, 0);
}

void CherryBomb::PlantUpdate()
{
	mBombTimer += DeltaTime::GetDeltaTime();
	if (mBombTimer >= mBombTime)
	{
		mBombTimer = 0.0f;
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CHERRYBOMB, 0.4f);
	}
}
#include "Shroom.h"
#include "../Board.h"

void Shroom::SetupPlant()
{
	if (mIsPreview || !mBoard) return;
	
	if (GameAPP::GetInstance().GetBackgroundIsNight(mBoard->mBackGround))
	{
		this->SetSleepState(false);
	}
	else 
	{
		this->SetSleepState(true);
		this->PlayTrack("anim_sleep");
	}
}

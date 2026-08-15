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
	}
}

void Shroom::SetSleepState(bool sleep)
{
	if (mIsSleeping == sleep) return;
	Plant::SetSleepState(sleep);
	if (sleep) {
		PlayTrack(GetSleepTrackName());
		return;
	}
	mWakeUpTimer = 0.0f;
	OnWakeUp();
}

void Shroom::OnWakeUp()
{
	PlayTrack(GetAwakeIdleTrackName());
}

#include "CherryBomb.h"
#include "../Board.h"

void CherryBomb::SetupPlant()
{
	if (mIsPreview) return;
	this->PlayTrack("anim_explode", GameRandom::Range(0.34f, 0.45f), 0);
	mAnimator->AddFrameEvent(13, [this]() {
		mBoard->CreateBoom(GetPosition());
		Die();
		});
}

void CherryBomb::TakeDamage(int damage)
{
	this->SetGlowingTimer(0.1f);
	return;
}
#include "CherryBomb.h"
#include "Game/Board/Board.h"

void CherryBomb::SetupPlant()
{
	if (mIsPreview) return;
	this->PlayTrack("anim_explode", GameRandom::Range(0.34f, 0.45f), 0);
	mAnimator->AddFrameEvent(13, [this]() {
		Explode();
		});
}

void CherryBomb::TakeDamage(int /*damage*/, DamageSource /*source*/)
{
	this->SetGlowingTimer(0.1f);
	return;
}

void CherryBomb::ResolveGargantuarSmash()
{
	if (GetCurrentTrackName() == "anim_explode") {
		Explode();
		return;
	}
	Plant::ResolveGargantuarSmash();
}

void CherryBomb::Explode()
{
	if (!IsActive()) return;
	if (mBoard) mBoard->CreateBoom(GetPosition(), mRow);
	Die();
}

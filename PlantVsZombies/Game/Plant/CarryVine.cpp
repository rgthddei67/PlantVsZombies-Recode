#include "CarryVine.h"
#include "../../ResourceManager.h"
#include "../../ResourceKeys.h"
#include "Game/RenderOrder.h"

namespace {
	constexpr float kFeedbackSeconds = 0.65f; // 搬运后抓合余韵，游戏秒；不影响事务提交
	constexpr float kFeedbackFadeSeconds = 0.2f; // 余韵末段淡出，游戏秒
}

void CarryVine::SetupPlant()
{
	RemoveShadow();
	RemoveCollider();
	PlayTrack("anim_idle");
}

void CarryVine::BeginFeedback(const Vector& position)
{
	mFeedbackRemaining = kFeedbackSeconds;
	SetPosition(position);
	SetRenderOrder(LAYER_EFFECTS);
	PlayTrackOnce("anim_grab", "anim_idle", 2.0f);
}

void CarryVine::Update()
{
	Plant::Update();
	if (mFeedbackRemaining < 0.0f) return;
	mFeedbackRemaining = std::max(0.0f, mFeedbackRemaining - DeltaTime::GetDeltaTime());
	SetAlpha(std::min(1.0f, mFeedbackRemaining / kFeedbackFadeSeconds));
	if (mFeedbackRemaining <= 0.0f) Die();
}

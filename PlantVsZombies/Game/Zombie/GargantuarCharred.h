#pragma once

#include "../AnimatedObject.h"
#include "../GameObjectManager.h"

/** 巨人专属化灰残影；是否显示背上的小鬼由死亡瞬间的持有状态冻结。 */
class GargantuarCharred final : public AnimatedObject {
public:
	GargantuarCharred(Board* board, const Vector& position, bool showImp, int row)
		: AnimatedObject(ObjectType::OBJECT_ZOMBIE, board, position,
			AnimationType::ANIM_GARGANTUAR_CHARRED, ColliderType::BOX,
			Vector::zero(), Vector::zero(), 1.0f, "GargantuarCharred", true)
		, mShowImp(showImp)
	{
		SetSortingKey(row);
	}

	void Start() override
	{
		GameObject::Start();
		if (!mAnimator) {
			GameObjectManager::GetInstance().DestroyGameObject(this);
			return;
		}
		mAnimator->SetTrackVisible("imphead", mShowImp);
		mAnimator->SetTrackVisible("impblink", mShowImp);
		mAnimator->SetFrameRangeToDefault();
		mAnimator->AddFrameEvent(42, [this]() {
			GameObjectManager::GetInstance().DestroyGameObject(this);
		});
		SetCurrentFrame(0.0f);
		SetLoopType(PlayState::PLAY_ONCE);
		SetAnimationSpeed(1.0f);
	}

private:
	bool mShowImp = true;
};

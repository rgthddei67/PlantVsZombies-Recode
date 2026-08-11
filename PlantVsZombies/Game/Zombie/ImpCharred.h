#pragma once

#include "../AnimatedObject.h"
#include "../GameObjectManager.h"

/** 小鬼专属化灰残影；资源没有片段标记，播放完整时间线并在第 34 帧回收。 */
class ImpCharred final : public AnimatedObject {
public:
	ImpCharred(Board* board, const Vector& position, int row)
		: AnimatedObject(ObjectType::OBJECT_ZOMBIE, board, position,
			AnimationType::ANIM_IMP_CHARRED, ColliderType::BOX,
			Vector::zero(), Vector::zero(), 1.0f, "ImpCharred", true)
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
		mAnimator->SetFrameRangeToDefault();
		mAnimator->AddFrameEvent(34, [this]() {
			GameObjectManager::GetInstance().DestroyGameObject(this);
		});
		SetCurrentFrame(0.0f);
		SetLoopType(PlayState::PLAY_ONCE);
		SetAnimationSpeed(1.0f);
	}
};

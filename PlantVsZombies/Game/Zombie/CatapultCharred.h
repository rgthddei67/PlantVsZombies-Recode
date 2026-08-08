#pragma once

#include "../AnimatedObject.h"
#include "../GameObjectManager.h"

/**
 * @brief 投篮车专属灰烬；资源没有 anim_crumble 标记，因此播放完整时间线并在第 29 帧回收。
 */
class CatapultCharred final : public AnimatedObject {
public:
	using AnimatedObject::AnimatedObject;

	void Start() override
	{
		GameObject::Start();
		if (!mAnimator) {
			GameObjectManager::GetInstance().DestroyGameObject(this);
			return;
		}
		mAnimator->SetFrameRangeToDefault();
		mAnimator->AddFrameEvent(29, [this]() {
			GameObjectManager::GetInstance().DestroyGameObject(this);
		});
		SetCurrentFrame(0.0f);
		SetLoopType(PlayState::PLAY_ONCE);
		SetAnimationSpeed(1.0f);
	}
};

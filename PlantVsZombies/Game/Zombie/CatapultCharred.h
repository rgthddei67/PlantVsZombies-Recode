#pragma once

#include "../AnimatedObject.h"
#include "../GameObjectManager.h"

/**
 * @brief 投篮车专属灰烬；资源没有 anim_crumble 标记，因此播放完整时间线并在第 29 帧回收。
 */
class CatapultCharred final : public AnimatedObject {
public:
	CatapultCharred(ObjectType type, Board* board, const Vector& position,
		AnimationType animType, const ColliderType& colliderType,
		const Vector& colliderSize, const Vector& colliderOffset, float scale,
		const std::string& tag, bool autoDestroy, int row)
		: AnimatedObject(type, board, position, animType, colliderType,
			colliderSize, colliderOffset, scale, tag, autoDestroy)
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
		mAnimator->AddFrameEvent(29, [this]() {
			GameObjectManager::GetInstance().DestroyGameObject(this);
		});
		SetCurrentFrame(0.0f);
		SetLoopType(PlayState::PLAY_ONCE);
		SetAnimationSpeed(1.0f);
	}
};

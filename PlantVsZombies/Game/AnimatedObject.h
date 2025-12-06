#pragma once
#ifndef _ANIMATED_OBJECT_H
#define _ANIMATED_OBJECT_H

#include "Component.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ReanimationComponent.h"
#include "ColliderComponent.h"
#include "../Reanimation/ReanimTypes.h"

class Board;

class AnimatedObject : public GameObject {
protected:
	Board* mBoard = nullptr;
	std::shared_ptr<TransformComponent> mTransform;
	std::shared_ptr<ReanimationComponent> mAnimation;
	std::shared_ptr<ColliderComponent> mCollider;

public:
	AnimatedObject(Board* board, const Vector& position, AnimationType animType,
		const ColliderType& colliderType,
		const Vector& colliderSize,
		float scale,
		const std::string& tag = "AnimatedObject",
		bool autoDestroy = true)
	{
		mBoard = board;
		SetTag(tag);

		mTransform = AddComponent<TransformComponent>();
		mTransform->position = position;

		mAnimation = AddComponent<ReanimationComponent>(animType, position, scale);
		if (mAnimation) {
			mAnimation->SetAutoDestroy(autoDestroy);
		}
		if (colliderSize.x > 0 && colliderSize.y > 0)
		{
			mCollider = AddComponent<ColliderComponent>(colliderSize, colliderType);
		}
	}

	void Start() override {
		GameObject::Start();
		this->PlayAnimation();
		this->SetAnimationSpeed(0.8f);
	}

	// 开始播放动画
	void PlayAnimation() {
		if (mAnimation) {
			mAnimation->Play();
		}
	}

	// 暂停动画播放 维持在暂停的这一帧
	void PauseAnimation()
	{
		if (mAnimation) {
			mAnimation->Pause();
		}
	}

	// 完全停止动画 并切换到第一帧
	void StopAnimation() {
		if (mAnimation) {
			mAnimation->Stop();
		}
	}

	// 设置动画位置
	void SetAnimationPosition(const Vector& position) {
		if (mTransform && mAnimation) {
			mTransform->position = position;
			mAnimation->SetPosition(position);
		}
	}

	// 检查动画是否完成
	bool IsAnimationFinished() const {
		return mAnimation ? mAnimation->IsFinished() : true;
	}

	// 设置自动销毁
	void SetAutoDestroy(bool autoDestroy) {
		if (mAnimation) {
			mAnimation->SetAutoDestroy(autoDestroy);
		}
	}

	// 设置循环类型
	void SetLoopType(PlayState loopType) {
		if (mAnimation) {
			mAnimation->SetLoopType(loopType);
		}
	}

	// 设置动画播放速度
	void SetAnimationSpeed(float speed) {
		if (mAnimation) {
			mAnimation->SetSpeed(speed);
		}
	}

	// 获取动画速度
	float GetAnimationSpeed() const {
		if (mAnimation) {
			return mAnimation->GetSpeed();
		}
	}

	// 获取动画组件
	std::shared_ptr<ReanimationComponent> GetAnimationComponent() const {
		return mAnimation;
	}

	// 获取变换组件
	std::shared_ptr<TransformComponent> GetTransformComponent() const {
		return mTransform;
	}

	// 获取碰撞组件
	std::shared_ptr<ColliderComponent> GetColliderComponent() const {
		return mCollider;
	}
};

#endif
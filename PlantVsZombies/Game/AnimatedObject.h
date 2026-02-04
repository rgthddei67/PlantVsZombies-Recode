#pragma once
#ifndef _ANIMATED_OBJECT_H
#define _ANIMATED_OBJECT_H

#include "Component.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ReanimationComponent.h"
#include "ColliderComponent.h"
#include "../Reanimation/ReanimTypes.h"
#include "../DeltaTime.h"
#include "../GameRandom.h"

class Board;

class AnimatedObject : public GameObject {
protected:
	Board* mBoard = nullptr;
	float mGlowingTimer = 0.0f;
	std::shared_ptr<Animator> mCachedAnimator = nullptr;
	std::weak_ptr<TransformComponent> mTransform;
	std::weak_ptr<ReanimationComponent> mAnimation;
	std::weak_ptr<ColliderComponent> mCollider;

public:
	AnimatedObject(ObjectType type, Board* board, const Vector& position, AnimationType animType,
		const ColliderType& colliderType,
		const Vector& colliderSize,
		const Vector& colliderOffset,
		float scale = 1.0f,
		const std::string& tag = "AnimatedObject",
		bool autoDestroy = true)
		: GameObject(type)
	{
		mBoard = board;
		SetTag(tag);

		mTransform = AddComponent<TransformComponent>();
		mTransform.lock()->position = position;

		mAnimation = AddComponent<ReanimationComponent>(animType, position, scale);
		if (auto animation = mAnimation.lock()) {
			animation->SetDrawOrder(80); // 确保动画在大多数组件之上绘制
			animation->SetAutoDestroy(autoDestroy);
		}
		if (colliderSize.x > 0 && colliderSize.y > 0)
		{
			mCollider = AddComponent<ColliderComponent>(colliderSize,
				colliderOffset, colliderType);
		}
	}

	void Start() override {
		GameObject::Start();
		this->PlayAnimation();
		this->SetAnimationSpeed(GameRandom::Range(0.65f, 0.85f));
		mCachedAnimator = mAnimation.lock()->GetAnimator();
	}

	void Update() override {
		GameObject::Update();

		if (mGlowingTimer > 0.0f) {
			mGlowingTimer -= DeltaTime::GetDeltaTime();

			if (mGlowingTimer > 0.0f) {
				mCachedAnimator->EnableGlowEffect(true);
			}
			else {
				mCachedAnimator->EnableGlowEffect(false);
			}
		}
	}

	// 开始播放动画
	void PlayAnimation() {
		if (auto animation = mAnimation.lock()) {
			animation->Play();
		}
	}

	// 暂停动画播放 维持在暂停的这一帧
	void PauseAnimation()
	{
		if (auto animation = mAnimation.lock()) {
			animation->Pause();
		}
	}

	// 完全停止动画 并切换到第一帧
	void StopAnimation() {
		if (auto animation = mAnimation.lock()) {
			animation->Stop();
		}
	}

	// 设置动画位置
	void SetAnimationPosition(const Vector& position) {
		if (auto transform = mTransform.lock())
		{
			if (auto animation = mAnimation.lock())
			{
				transform->position = position;
				animation->SetPosition(position);
			}
		}
	}

	// 检查动画是否完成
	bool IsAnimationFinished() const {
		if (auto animation = mAnimation.lock()) {
			return animation->IsFinished();
		}
	}

	// 设置自动销毁
	void SetAutoDestroy(bool autoDestroy) {
		if (auto animation = mAnimation.lock()) {
			animation->SetAutoDestroy(autoDestroy);
		}
	}

	// 设置循环类型
	void SetLoopType(PlayState loopType) {
		if (auto animation = mAnimation.lock()) {
			animation->SetLoopType(loopType);
		}
	}

	// 设置动画播放速度
	void SetAnimationSpeed(float speed) {
		if (auto animation = mAnimation.lock()) {
			animation->SetSpeed(speed);
		}
	}

	// 获取动画速度
	float GetAnimationSpeed() const {
		if (auto animation = mAnimation.lock()) {
			return animation->GetSpeed();
		}
	}

	// 获取动画组件
	std::shared_ptr<ReanimationComponent> GetAnimationComponent() const {
		return mAnimation.lock();
	}

	// 获取变换组件
	std::shared_ptr<TransformComponent> GetTransformComponent() const {
		return mTransform.lock();
	}

	// 获取碰撞组件
	std::shared_ptr<ColliderComponent> GetColliderComponent() const {
		return mCollider.lock();
	}
};

#endif
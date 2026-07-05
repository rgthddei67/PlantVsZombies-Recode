#pragma once
#ifndef _ANIMATED_OBJECT_H
#define _ANIMATED_OBJECT_H

#include "Component.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ColliderComponent.h"
#include "../Reanimation/Animator.h"
#include "../Reanimation/ReanimTypes.h"
#include "../ResourceManager.h"
#include <memory>

class Board;

class AnimatedObject : public GameObject {
protected:
	Board* mBoard = nullptr;
	float mGlowingTimer = 0.0f;

	TransformComponent* mTransform = nullptr;
	ColliderComponent* mCollider = nullptr;
	std::shared_ptr<Animator> mAnimator;

	AnimationType mAnimType = AnimationType::ANIM_NONE;
	bool mIsPlaying = false;
	PlayState mLoopType = PlayState::PLAY_REPEAT;
	bool mAutoDestroy = true;
	bool mAdvancedInParallel = false;   // 阶段二：本帧 animator 是否已在并行段推进过

public:
	AnimatedObject(ObjectType type,
		Board* board,
		const Vector& position,
		AnimationType animType,
		const ColliderType& colliderType = ColliderType::BOX,
		const Vector& colliderSize = Vector::zero(),
		const Vector& colliderOffset = Vector::zero(),
		float scale = 1.0f,
		const std::string& tag = "AnimatedObject",
		bool autoDestroy = true);

	// 动画控制
	void PlayAnimation();
	void PauseAnimation();
	void StopAnimation();

	void SetAnimationPosition(const Vector& position);
	Vector GetAnimationPosition() const;
	void SetAnimationScale(float scale);
	float GetAnimationScale() const;
	bool IsAnimationFinished() const;
	bool IsAnimationPlaying() const;

	void SetAutoDestroy(bool autoDestroy);
	void SetLoopType(PlayState loopType);
	void SetAnimationSpeed(float speed);
	float GetAnimationSpeed() const;
	void SetAlpha(float alpha);
	float GetAlpha() const;
	void SetClipSpeed(float speed);
	float GetClipSpeed() const;

	// 轨道附加
	bool AttachAnimatorToTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator);
	void DetachAnimatorFromTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator);
	void DetachAllAnimators();

	// 视觉效果
	void SetGlowingTimer(float duration);
	void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128);
	void EnableGlowEffect(bool enable);
	void EnableOverlayEffect(bool enable);
	void OverrideColor(const SDL_Color& color);

	// 轨道播放
	bool PlayTrack(const std::string& trackName, float speed = 0.0f, float blendTime = 0.0f);
	bool PlayTrackOnce(const std::string& trackName,
		const std::string& returnTrack = "",
		float speed = 0.0f,
		float blendTime = 0.0f,
		float returnSpeed = 0.0f);

	void SetFramesForLayer(const std::string& trackName);

	// 存档/读档辅助
	std::string GetCurrentTrackName() const;
	float GetCurrentFrame() const;
	void SetCurrentFrame(float frameIndex);

	// 播放状态机 (供 GameInfoSaver 完整持久化 PlayTrackOnce，避免读档后一次性动画死循环)
	PlayState GetPlayingState() const;
	std::string GetTargetTrack() const;
	float GetTargetTrackSpeed() const;

	// 组件获取
	TransformComponent* GetTransformComponent() const;
	ColliderComponent* GetColliderComponent() const;

	// 获取视觉绘制位置
	virtual Vector GetVisualPosition() const;

	std::shared_ptr<Animator> GetAnimatorInternal() const;

	void Update() override;
	void UpdateParallel(std::vector<DeferredEvent>& outBuf) override;
	void Draw(Graphics* g) override;

private:
	void UpdateGlowingEffect();
};

#endif
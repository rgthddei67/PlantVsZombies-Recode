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

    std::weak_ptr<TransformComponent> mTransform;
    std::weak_ptr<ColliderComponent> mCollider;
    std::shared_ptr<Animator> mAnimator;

    AnimationType mAnimType;
    bool mIsPlaying;
    PlayState mLoopType;
    bool mAutoDestroy;

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
    void SetOriginalSpeed(float speed);
    float GetOriginalSpeed();

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
    bool PlayTrack(const std::string& trackName, float speed = 1.0f, float blendTime = 0.0f);
    bool PlayTrackOnce(const std::string& trackName,
        const std::string& returnTrack = "",
        float speed = 1.0f,
        float blendTime = 0.0f);

    void SetFramesForLayer(const std::string& trackName);

    // 组件获取
    std::shared_ptr<TransformComponent> GetTransformComponent() const;
    std::shared_ptr<ColliderComponent> GetColliderComponent() const;

    // 获取视觉绘制位置
    virtual Vector GetVisualPosition() const;

    void Update() override;
    void Draw(Graphics* g) override;

private:
    void UpdateGlowingEffect();
    std::shared_ptr<Animator> GetAnimatorInternal() const;
};

#endif
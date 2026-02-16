#pragma once
#ifndef _ANIMATED_OBJECT_H
#define _ANIMATED_OBJECT_H

#include "Component.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ReanimationComponent.h"
#include "ColliderComponent.h"
#include "../Reanimation/ReanimTypes.h"
#include <memory>

class Board;

class AnimatedObject : public GameObject {
protected:
    Board* mBoard = nullptr;
    float mGlowingTimer = 0.0f;
    std::weak_ptr<TransformComponent> mTransform;
    std::weak_ptr<ReanimationComponent> mAnimation;
    std::weak_ptr<ColliderComponent> mCollider;

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

    /**
     * @brief 将另一个动画器附加到本对象的指定轨道上
     * @param trackName 轨道名称（如 "anim_stem"）
     * @param childAnimator 子动画器
     * @return 是否成功
     */
    bool AttachAnimatorToTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator);

    /**
     * @brief 从轨道分离子动画器
     */
    void DetachAnimatorFromTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator);

    /**
     * @brief 分离所有附加的子动画器
     */
    void DetachAllAnimators();

    void SetGlowingTimer(float duration);
    void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128);
    void EnableGlowEffect(bool enable);
    void EnableOverlayEffect(bool enable);
    void OverrideColor(const SDL_Color& color);

    bool PlayTrack(const std::string& trackName, float blendTime = 0);
    bool PlayTrackOnce(const std::string& trackName, const std::string& returnTrack = "", float speed = 1.0f, float blendTime = 0);
    void SetFramesForLayer(const std::string& trackName);

    std::shared_ptr<ReanimationComponent> GetAnimationComponent() const;
    std::shared_ptr<TransformComponent> GetTransformComponent() const;
    std::shared_ptr<ColliderComponent> GetColliderComponent() const;
    std::shared_ptr<Animator> GetAnimator() const;

    void Start() override;
    void Update() override;

private:
    void UpdateGlowingEffect();
    std::shared_ptr<Animator> GetAnimatorInternal() const;
};

#endif
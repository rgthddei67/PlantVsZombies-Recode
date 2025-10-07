#pragma once
#ifndef _ANIMATED_OBJECT_H
#define _ANIMATED_OBJECT_H

#include "Component.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ReanimationComponent.h"
#include "ColliderComponent.h"

class AnimatedObject : public GameObject {
protected:
    std::shared_ptr<TransformComponent> transform;
    std::shared_ptr<ReanimationComponent> animation;
    std::shared_ptr<ColliderComponent> collider;

public:
    AnimatedObject(const Vector& position, AnimationType animType,
        const Vector& colliderSize = Vector(50, 50),
		float scale = 1.0f, bool needCollider = true,
        const std::string& tag = "AnimatedObject",
        bool autoDestroy = false) {

        SetTag(tag);

        transform = AddComponent<TransformComponent>();
        transform->position = position;

        animation = AddComponent<ReanimationComponent>(animType, position, scale);
        if (animation) {
            animation->SetAutoDestroy(autoDestroy);
        }
        if (needCollider)
        {
            if (colliderSize.x > 0 && colliderSize.y > 0)
            {
                collider = AddComponent<ColliderComponent>(colliderSize);
            }
        }
    }

    // 开始播放动画
    void PlayAnimation() {
        if (animation) {
            animation->Play();
        }
    }

    // 停止动画
    void StopAnimation() {
        if (animation) {
            animation->Stop();
        }
    }

    // 设置动画位置
    void SetAnimationPosition(const Vector& position) {
        if (transform && animation) {
            transform->position = position;
            animation->SetPosition(position);
        }
    }

    // 检查动画是否完成
    bool IsAnimationFinished() const {
        return animation ? animation->IsFinished() : true;
    }

    // 设置自动销毁
    void SetAutoDestroy(bool autoDestroy) {
        if (animation) {
            animation->SetAutoDestroy(autoDestroy);
        }
    }

    // 设置循环类型
    void SetLoopType(ReanimLoopType loopType) {
        if (animation) {
            animation->SetLoopType(loopType);
        }
    }

    // 获取动画组件
    std::shared_ptr<ReanimationComponent> GetAnimationComponent() const {
        return animation;
    }
};

#endif
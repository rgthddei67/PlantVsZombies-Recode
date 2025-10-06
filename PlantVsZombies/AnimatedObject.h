#pragma once
#ifndef _ANIMATED_OBJECT_H
#define _ANIMATED_OBJECT_H

#include "Component.h"
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
        const std::string& tag = "AnimatedObject") {

        SetTag(tag);

        transform = AddComponent<TransformComponent>();
        transform->position = position;

        animation = AddComponent<ReanimationComponent>(animType, position, scale);

        if (needCollider)
        {
            if (colliderSize.x > 0 && colliderSize.y > 0)
            {
                collider = AddComponent<ColliderComponent>(colliderSize);
            }
        }
    }

    // ��ʼ���Ŷ���
    void PlayAnimation() {
        if (animation) {
            animation->Play();
        }
    }

    // ֹͣ����
    void StopAnimation() {
        if (animation) {
            animation->Stop();
        }
    }

    // ���ö���λ��
    void SetAnimationPosition(const Vector& position) {
        if (transform && animation) {
            transform->position = position;
            animation->SetPosition(position);
        }
    }

    // ��鶯���Ƿ����
    bool IsAnimationFinished() const {
        return animation ? animation->IsFinished() : true;
    }

    // ��ȡ�������
    std::shared_ptr<ReanimationComponent> GetAnimationComponent() const {
        return animation;
    }
};

#endif
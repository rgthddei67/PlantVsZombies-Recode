#pragma once
#ifndef _COIN_H
#define _COIN_H

#include "AnimatedObject.h"
#include "ClickableComponent.h"
#include "AudioSystem.h"
#include "GameObjectManager.h"

class Coin : public AnimatedObject {
protected:
    Vector targetPos = Vector(10, 10);  // Ŀ��λ��
    float speedFast = 500.0f;           // ���ٽ׶��ٶ�
    float speedSlow = 320.0f;           // ���ٽ׶��ٶ�
    float slowDownDistance = 0.5f;     // ��ʼ���ٵľ�����ֵ
    bool isMovingToTarget = false;      // �Ƿ������ƶ���Ŀ��λ��

public:
    Coin(AnimationType animType, const Vector& position,
        const Vector& colliderSize, float scale = 1.0f,
        const std::string& tag = "Coin", bool autoDestroy = true)
        : AnimatedObject(position, animType, ColliderType::CIRCLE, colliderSize, scale, tag, autoDestroy)
    {
    }
    void Start() override
    {
		AnimatedObject::Start();
        auto clickableComponent = AddComponent<ClickableComponent>();
        SetOnClickBack(clickableComponent);
    }

    void Update() override
    {
        AnimatedObject::Update();

        if (!isMovingToTarget) return;

        Vector currentPos = GetPosition();
        Vector direction = targetPos - currentPos;
        float distance = direction.magnitude();

        // ����Ѿ�����Ŀ��λ��
        if (distance < 75.0f)
        {
            OnReachTargetBack();
            return;
        }

        // ���㵱ǰ�ٶȣ��ȿ������
        float currentSpeed = (distance > slowDownDistance) ? speedFast : speedSlow;

        // ��ȷ�Ĺ�һ������
        if (distance > 0) {
            Vector normalizedDir = direction / distance;
            Vector newPos = currentPos + normalizedDir * currentSpeed * 0.016f;
            SetPosition(newPos);
        }
    }

    virtual void SetOnClickBack(std::shared_ptr<ClickableComponent> clickComponent)
    {
        if (clickComponent == nullptr) return;
        clickComponent->onClick = [this]() {
            StartMoveToTarget();
        };
    }

    // ��ʼ�ƶ���Ŀ��λ��
    void StartMoveToTarget(const Vector& target = Vector(10, 10), float fastSpeed = 500.0f, float slowSpeed = 100.0f, float slowdownDist = 80.0f)
    {
		StopAnimation();
        targetPos = target;
        speedFast = fastSpeed;
        speedSlow = slowSpeed;
        slowDownDistance = slowdownDist;
        isMovingToTarget = true;
    }

    // ֹͣ�ƶ�
    void StopMove()
    {
        isMovingToTarget = false;
    }

    // �Ƿ������ƶ�
    bool IsMoving() const
    {
        return isMovingToTarget;
    }

    // ����Ŀ��λ��
    void SetTargetPosition(const Vector& target)
    {
        targetPos = target;
    }

    // ����Ŀ��ʱ�Ļص�
    virtual void OnReachTargetBack()
    {
        isMovingToTarget = false;
        GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
    }

protected:
    Vector GetPosition() const
    {
        if (transform) {
            return transform->position;
        }
        return Vector::zero();
    }

    void SetPosition(const Vector& newPos)
    {
        if (transform) {
            transform->SetPosition(newPos);
        }
    }
};
#endif
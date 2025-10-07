#pragma once
#ifndef _COIN_H
#define _COIN_H

#include "AnimatedObject.h"
#include "ClickableComponent.h"
#include "AudioSystem.h"
#include "GameObjectManager.h"

class Coin : public AnimatedObject {
protected:
    Vector targetPos = Vector(10, 10);  // 目标位置
    float speedFast = 500.0f;           // 快速阶段速度
    float speedSlow = 320.0f;           // 慢速阶段速度
    float slowDownDistance = 0.5f;     // 开始减速的距离阈值
    bool isMovingToTarget = false;      // 是否正在移动到目标位置

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

        // 如果已经到达目标位置
        if (distance < 75.0f)
        {
            OnReachTargetBack();
            return;
        }

        // 计算当前速度（先快后慢）
        float currentSpeed = (distance > slowDownDistance) ? speedFast : speedSlow;

        // 正确的归一化操作
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

    // 开始移动到目标位置
    void StartMoveToTarget(const Vector& target = Vector(10, 10), float fastSpeed = 500.0f, float slowSpeed = 100.0f, float slowdownDist = 80.0f)
    {
		StopAnimation();
        targetPos = target;
        speedFast = fastSpeed;
        speedSlow = slowSpeed;
        slowDownDistance = slowdownDist;
        isMovingToTarget = true;
    }

    // 停止移动
    void StopMove()
    {
        isMovingToTarget = false;
    }

    // 是否正在移动
    bool IsMoving() const
    {
        return isMovingToTarget;
    }

    // 设置目标位置
    void SetTargetPosition(const Vector& target)
    {
        targetPos = target;
    }

    // 到达目标时的回调
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
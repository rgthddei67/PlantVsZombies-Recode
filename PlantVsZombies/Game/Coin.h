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
    float speedSlow = 180.0f;           // 慢速阶段速度
    float slowDownDistance = 20.0f;     // 开始减速的距离阈值
    bool isMovingToTarget = false;      // 是否正在移动到目标位置

public:
public:
    Coin(Board* board, AnimationType animType, const Vector& position,
        const Vector& colliderSize, float scale = 1.0f,
        const std::string& tag = "Coin", bool autoDestroy = true);

    void Start() override;
    void Update() override;

    virtual void SetOnClickBack(std::shared_ptr<ClickableComponent> clickComponent);

    void StartMoveToTarget(const Vector& target = Vector(10, 10),
        float fastSpeed = 500.0f,
        float slowSpeed = 100.0f,
        float slowdownDist = 80.0f);

    void StopMove();
    bool IsMoving() const;
    void SetTargetPosition(const Vector& target);
    virtual void OnReachTargetBack();

protected:
    Vector GetPosition() const;
    void SetPosition(const Vector& newPos);
};
#endif
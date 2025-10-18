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
    float speedSlow = 180.0f;           // ���ٽ׶��ٶ�
    float slowDownDistance = 20.0f;     // ��ʼ���ٵľ�����ֵ
    bool isMovingToTarget = false;      // �Ƿ������ƶ���Ŀ��λ��

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
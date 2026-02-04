#pragma once
#ifndef _COIN_H
#define _COIN_H

#include "AnimatedObject.h"
#include "ClickableComponent.h"
#include "AudioSystem.h"
#include "GameObjectManager.h"

class Coin : public AnimatedObject {
protected:
    float mVanlishTime = 15.0f;           // 消失时间
    float mVanlishTimer = 0.0f;           // 消失计时器
    Vector targetPos = Vector(10, 10);    // 目标位置
    float speedFast = 500.0f;             // 快速阶段速度
    float speedSlow = 180.0f;             // 慢速阶段速度
    float slowDownDistance = 20.0f;       // 开始减速的距离阈值
    bool isMovingToTarget = false;        // 是否正在移动到目标位置

    float mScaleTimer = 0.0f;             // 缩放计时器
    float mScaleDuration = 0.9f;          // 缩放持续时间
    float mStartScale = 0.1f;             // 起始缩放
    float mTargetScale = 1.0f;            // 目标缩放
    bool mIsScaling = true;               // 是否正在缩放
    bool mScaleAnimationFinished = false; // 缩放动画是否已完成

	bool mIsDestroyed = false;            // 是否已销毁

public:
    int mCoinID = 0;

public:
    Coin(Board* board, AnimationType animType, const Vector& position, 
        const Vector& colliderSize, const Vector& colliderOffset, float VanlishTime, float scale = 1.0f,
        const std::string& tag = "Coin", bool needScaleAnimation = false, bool autoDestroy = true);

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

    void SetScale(float scale);
    void UpdateScale();
    void StopScaleAnimation();
    void FinishScaleAnimation();
};

#endif
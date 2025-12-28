#pragma once
#ifndef _SUN_H
#define _SUN_H
#include "../DeltaTime.h"
#include "../GameRandom.h"
#include "Coin.h"
#include "Board.h"

class Sun : public Coin {
private:
	int SunPoint = 25;	// 收集后增加的阳光点数
    Vector mParabolaTarget; // 抛物线运动的目标位置
    Vector mParabolaStart;  // 抛物线起始位置
    float mParabolaHeight = 0.0f;  // 抛物线高度
    float mParabolaTime = 0.0f; // 抛物线运动时间
    bool mIsParabola = false; // 是否正在进行抛物线运动
    bool mShouldStartLinearFall = false; // 是否需要开始匀速下落
    bool mShouldStartParabolaFall = false; // 是否需要开始抛物线运动
    bool mIsLinearFalling = false; // 是否正在进行匀速下落
    Vector mLinearTarget; // 匀速下落的目标位置
    bool mIsInitialized = false; // 是否已初始化

public:
	Sun(Board* board, const Vector& position, float scale = 0.9f,
		const std::string& tag = "Sun", bool needScaleAnimation = false, bool autoDestroy = true)
		: Coin(board, AnimationType::ANIM_SUN, position,
			Vector(65, 65), 10.0f, scale, tag, needScaleAnimation, autoDestroy)
	{
		speedFast = 700.0f;
		speedSlow = 500.0f;
		slowDownDistance = 130.0f;
	}

	// TODO 析构函数会自动调用基类的析构函数 不能自己写
    ~Sun() override {
        this->mIsParabola = false;
        this->mIsLinearFalling = false;
    }

    void Start() override {
        Coin::Start();
        mIsInitialized = true;

        if (mShouldStartLinearFall) {
            mShouldStartLinearFall = false;
            StartLinearFall();
        }
        if (mShouldStartParabolaFall) {
            mShouldStartParabolaFall = false;
            StartParabolaFall();
        }
    }

	void OnReachTargetBack() override
	{
		Coin::OnReachTargetBack();
		mBoard->AddSun(SunPoint);
	}

    void SetOnClickBack(std::shared_ptr<ClickableComponent> clickComponent) override
    {
        if (clickComponent == nullptr) return;
        clickComponent->onClick = [this]() {
			AudioSystem::PlaySound(AudioConstants::SOUND_COLLECT_SUN, 0.5f);
			StartMoveToTarget(Vector(10, 10), speedFast, speedSlow, slowDownDistance);
        };
    }

    // 匀速下落
    void StartLinearFall() {
        // 检查组件是否已初始化
        if (!mIsInitialized) {
            mShouldStartLinearFall = true;
            return;
        }

        Vector currentPos = GetPosition();
        // 目标位置：x不变，y在80~420之间随机
		float targetY = GameRandom::Range(80.0f, 420.0f);
        mLinearTarget = Vector(currentPos.x, targetY);
        mIsLinearFalling = true;
    }

    // 抛物线运动
    void StartParabolaFall() {
        // 检查组件是否已初始化
        if (!mIsInitialized) {
            mShouldStartParabolaFall = true;
            return;
        }

        mParabolaStart = GetPosition();

        // 水平方向随机偏移 -50~50
		float offsetX = GameRandom::Range(-50.0f, 50.0f);

        // 下落距离 20~50
        float fallDistance = GameRandom::Range(20.0f, 50.0f);

        mParabolaTarget = Vector(mParabolaStart.x + offsetX, mParabolaStart.y + fallDistance);

        // 抛物线高度（基于下落距离计算）
        mParabolaHeight = fallDistance * 0.8f;

        mIsParabola = true;
        mParabolaTime = 0.0f;
    }

    void UpdateLinearFall() {
        if (!mIsLinearFalling) return;

        Vector currentPos = GetPosition();
        Vector direction = mLinearTarget - currentPos;
        float distance = direction.magnitude();

        // 如果到达目标位置，停止下落
        if (distance < 1.0f) {
            mIsLinearFalling = false;
            // 到达位置后保持可点击状态
            return;
        }

        // 匀速下落速度 - 适当增加速度
        float speed = 50.0f; // 增加下落速度

        if (distance > 0) {
            Vector normalizedDir = direction / distance;
            Vector newPos = currentPos + normalizedDir * speed * DeltaTime::GetDeltaTime();
            SetPosition(newPos);
        }
    }

    // 更新抛物线运动
    void UpdateParabola() {
        if (!mIsParabola) return;

        mParabolaTime += DeltaTime::GetDeltaTime();

        // 抛物线运动时间（根据距离调整）
        float totalTime = 2.0f; // 增加运动时间

        if (mParabolaTime >= totalTime) {
            // 运动结束
            mIsParabola = false;
            SetPosition(mParabolaTarget);
            return;
        }

        // 计算进度 (0~1)
        float t = mParabolaTime / totalTime;

        // 水平方向匀速运动
        float currentX = mParabolaStart.x + (mParabolaTarget.x - mParabolaStart.x) * t;

        // 垂直方向抛物线运动
        float currentY = mParabolaStart.y + (mParabolaTarget.y - mParabolaStart.y) * t
            - mParabolaHeight * 4.0f * t * (1.0f - t);

        SetPosition(Vector(currentX, currentY));
    }

    void Update() override {
        Coin::Update();
        if (isMovingToTarget) return;
        if (mIsParabola) {
            UpdateParabola();
        }
        else if (mIsLinearFalling) {
            UpdateLinearFall();
        }
    }

};
#endif
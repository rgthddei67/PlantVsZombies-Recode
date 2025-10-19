#pragma once
#ifndef _SUN_H
#define _SUN_H
#include "Coin.h"

class Sun : public Coin {
private:
	int SunPoint = 25;	// 收集后增加的阳光点数
    Vector mParabolaTarget; // 抛物线运动的目标位置
    Vector mParabolaStart;  // 抛物线起始位置
    float mParabolaHeight;  // 抛物线高度
    float mParabolaTime = 0.0f; // 抛物线运动时间
    bool mIsParabola = false; // 是否正在进行抛物线运动
    bool mShouldStartLinearFall = false; // 标记是否需要开始匀速下落
    bool mShouldStartParabolaFall = false; // 标记是否需要开始抛物线运动
    bool mIsLinearFalling = false; // 标记是否正在进行匀速下落
    Vector mLinearTarget; // 匀速下落的目标位置

public:
	Sun(Board* board, const Vector& position, float scale = 0.75f,
		const std::string& tag = "Sun", bool autoDestroy = true)
		: Coin(board, AnimationType::ANIM_SUN, position,
			Vector(65, 65), scale, tag, autoDestroy)
	{
		speedFast = 700.0f;
		speedSlow = 500.0f;
		slowDownDistance = 130.0f;
	}

    void Start() override {
        Coin::Start(); 

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
        if (!started) {
            mShouldStartLinearFall = true;
            return;
        }

        Vector currentPos = GetPosition();
        // 目标位置：x不变，y在80~420之间随机
        float targetY = 80.0f + static_cast<float>(rand() % 341); // 80~420
        mLinearTarget = Vector(currentPos.x, targetY);
        mIsLinearFalling = true;

        // 确保动画在播放
        PlayAnimation();
    }

    // 抛物线运动
    void StartParabolaFall() {
        // 检查组件是否已初始化
        if (!started) {
            mShouldStartParabolaFall = true;
            return;
        }

        mParabolaStart = GetPosition();

        // 水平方向随机偏移 -50~50
        float offsetX = static_cast<float>(-50 + rand() % 101);

        // 下落距离 20~50
        float fallDistance = 20.0f + static_cast<float>(rand() % 31);

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
            // 到达位置后不要销毁对象，保持可点击状态
            return;
        }

        // 匀速下落速度
        float speed = 50.0f; // 可以根据需要调整

        if (distance > 0) {
            Vector normalizedDir = direction / distance;
            Vector newPos = currentPos + normalizedDir * speed * DELTA_TIME;
            SetPosition(newPos);
        }
    }

    // 更新抛物线运动
    void UpdateParabola() {
        if (!mIsParabola) return;

        mParabolaTime += DELTA_TIME;

        // 抛物线运动时间（根据距离调整）
        float totalTime = 1.5f;

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
        if (mIsParabola) {
            UpdateParabola();
        }
        else if (mIsLinearFalling) {
            UpdateLinearFall();
        }
        else {
            Coin::Update();
        }
    }

};
#endif
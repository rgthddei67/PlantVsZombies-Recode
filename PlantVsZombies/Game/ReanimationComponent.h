#pragma once
#ifndef _REANIMATION_COMPONENT_H
#define _REANIMATION_COMPONENT_H

#include "../Game/GameObjectManager.h"
#include "../ResourceManager.h"
#include "../Game/Component.h"
#include "../Reanimation/Animator.h"
#include "../Reanimation/AnimationTypes.h"
#include <memory>

class GameObjectManager;

class ReanimationComponent : public Component {
private:
    std::shared_ptr<Animator> mAnimator;
    AnimationType mAnimType;
    Vector mPosition;
    float mScale;
    bool mIsPlaying;
    PlayState mLoopType;
    bool mAutoDestroy;

public:
    ReanimationComponent(AnimationType animType, const Vector& position = Vector::zero(), float scale = 1.0f)
        : mAnimType(animType), mPosition(position), mScale(scale), mIsPlaying(false),
        mLoopType(PlayState::PLAY_REPEAT), mAutoDestroy(true) {
    }

    ~ReanimationComponent() override {

    }

    void Start() override {
        auto gameObj = GetGameObject();
        if (!gameObj) return;

        // 获取动画资源
        ResourceManager& resMgr = ResourceManager::GetInstance();
        auto reanimResource =
            resMgr.GetReanimation(resMgr.AnimationTypeToString(mAnimType));

        if (!reanimResource) {
            std::cout << "ReanimationComponent::Start failed: Could not load animation resource for type "
                << static_cast<int>(mAnimType) << std::endl;
            return;
        }

        // 创建动画控制器
        mAnimator = std::make_shared<Animator>(reanimResource);

        // 设置初始位置
        if (auto transformComp = gameObj->GetComponent<TransformComponent>()) {
            mPosition = transformComp->position;
        }

        // 设置动画参数
        mAnimator->SetSpeed(1.0f);

        // 根据循环类型设置播放状态
        mAnimator->Play(mLoopType);

        mIsPlaying = true;
    }

    void Update() override {
        if (!mAnimator) return;

        // 更新位置
        if (auto gameObj = GetGameObject()) {
            if (auto transformComp = gameObj->GetComponent<TransformComponent>()) {
                mPosition = transformComp->position;
                mScale = transformComp->GetScale();
            }
        }

        // 更新动画
        mAnimator->Update();

        if (!mIsPlaying) return;

        // 检查动画是否完成
        if (mLoopType != PlayState::PLAY_REPEAT && IsFinished()) {
            if (mAutoDestroy) {
                if (auto gameObj = GetGameObject()) {
                    GameObjectManager::GetInstance().DestroyGameObject(gameObj);
                }
            }
            else {
                mIsPlaying = false;
            }
        }
    }

    void Draw(SDL_Renderer* renderer) override {
        if (!mAnimator) return;

        mAnimator->Draw(renderer, mPosition.x, mPosition.y, mScale);
    }

    // 开始播放
    void Play() {
        mIsPlaying = true;
        if (mAnimator) {
            mAnimator->Play(mLoopType);
        }
    }

    // 完全停止播放并切换到第一帧
    void Stop() {
        mIsPlaying = false;
        if (mAnimator) {
            mAnimator->Stop();
        }
    }

    // 在当前播放位置暂停播放
    void Pause()
    {
        mIsPlaying = false;
        if (mAnimator) {
            mAnimator->Pause();
        }
    }

    void SetLoopType(PlayState loopType) {
        mLoopType = loopType;
        // 如果正在播放，重新设置播放状态
        if (mIsPlaying && mAnimator) {
            mAnimator->Play(mLoopType);
        }
    }

    void SetPosition(const Vector& position) {
        mPosition = position;
    }

    Vector GetPosition() const {
        return mPosition;
    }

    void SetScale(float scale) {
        mScale = scale;
    }

    float GetScale() const {
        return mScale;
    }

    // 设置透明度
    void SetAlpha(float alpha) {
        if (mAnimator) {
            mAnimator->SetAlpha(alpha);
        }
    }

    // 获取透明度
    float GetAlpha() const {
        return mAnimator ? mAnimator->GetAlpha() : 1.0f;
    }

    bool IsPlaying() const {
        return mIsPlaying && mAnimator && mAnimator->IsPlaying();
    }

    bool IsFinished() const {
        if (!mAnimator) return true;

        if (mLoopType == PlayState::PLAY_REPEAT) return false;

        return !mAnimator->IsPlaying();
    }

    void SetAutoDestroy(bool autoDestroy) {
        mAutoDestroy = autoDestroy;
    }

    bool GetAutoDestroy() const {
        return mAutoDestroy;
    }

    // 设置特定轨道的帧范围
    void SetFramesForLayer(const std::string& trackName) {
        if (mAnimator) {
            mAnimator->SetFrameRangeByTrackName(trackName);
        }
    }

    // 查找轨道索引
    int FindTrackIndex(const std::string& trackName) {
        if (!mAnimator) return -1;

        auto reanim = mAnimator->GetReanimation();
        if (reanim) {
            return reanim->FindTrackIndex(trackName);
        }
        return -1;
    }

    // 获取底层 Animator 对象
    std::shared_ptr<Animator> GetAnimator() const {
        return mAnimator;
    }

    // 获取动画类型
    AnimationType GetAnimationType() const {
        return mAnimType;
    }

    // 设置动画速度
    void SetSpeed(float speed) {
        if (mAnimator) {
            mAnimator->SetSpeed(speed);
        }
    }

    float GetSpeed() const {
        return mAnimator ? mAnimator->GetSpeed() : 0.0f;
    }

};

#endif
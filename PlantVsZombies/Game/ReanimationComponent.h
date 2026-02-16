#pragma once
#ifndef _REANIMATION_COMPONENT_H
#define _REANIMATION_COMPONENT_H

#include "../Game/GameObjectManager.h"
#include "../ResourceManager.h"
#include "../Game/Component.h"
#include "../Reanimation/Animator.h"
#include "../Reanimation/AnimationTypes.h"
#include "../DeltaTime.h"
#include <memory>
#include <vector>

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

    ~ReanimationComponent() override = default;

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

        mAnimator = std::make_shared<Animator>(reanimResource);

        // 设置初始位置
        if (auto transformComp = gameObj->GetComponent<TransformComponent>()) {
            mPosition = transformComp->position;
            mScale = transformComp->GetScale();
        }

        // 设置动画参数
        mAnimator->SetSpeed(1.0f);
        mAnimator->SetAlpha(1.0f);
        mAnimator->Play(mLoopType);

        mIsPlaying = true;
    }

    void Update() override {
        if (!mAnimator) return;

        // 更新位置和缩放
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

    /**
     * @brief 将另一个动画附加到本动画的指定轨道上（跟随轨道变换）
     * @param trackName 轨道名称（如 "anim_stem"）
     * @param childAnimator 要附加的子动画器
     * @return 是否成功
     */
    bool AttachAnimatorToTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator) {
        if (!mAnimator || !childAnimator) return false;
        return mAnimator->AttachAnimator(trackName, childAnimator);
    }

    /**
     * @brief 从轨道分离子动画
     */
    void DetachAnimatorFromTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator) {
        if (mAnimator) {
            mAnimator->DetachAnimator(trackName, childAnimator);
        }
    }

    /**
     * @brief 分离所有附加的子动画
     */
    void DetachAllAnimators() {
        if (mAnimator) {
            mAnimator->DetachAllAnimators();
        }
    }

    void Play() {
        mIsPlaying = true;
        if (mAnimator) {
            mAnimator->Play(mLoopType);
        }
    }

    void Stop() {
        mIsPlaying = false;
        if (mAnimator) {
            mAnimator->Stop();
        }
    }

    void Pause() {
        mIsPlaying = false;
        if (mAnimator) {
            mAnimator->Pause();
        }
    }

    void SetLoopType(PlayState loopType) {
        mLoopType = loopType;
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

    void SetAlpha(float alpha) {
        if (mAnimator) {
            mAnimator->SetAlpha(alpha);
        }
    }

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

    void SetFramesForLayer(const std::string& trackName) {
        if (mAnimator) {
            mAnimator->SetFrameRangeByTrackName(trackName);
        }
    }

    std::shared_ptr<Animator> GetAnimator() const {
        return mAnimator;
    }

    AnimationType GetAnimationType() const {
        return mAnimType;
    }

    void SetSpeed(float speed) {
        if (mAnimator) {
            mAnimator->SetSpeed(speed);
        }
    }

    float GetSpeed() const {
        return mAnimator ? mAnimator->GetSpeed() : 0.0f;
    }

    void EnableGlowEffect(bool enable) {
        if (mAnimator) {
            mAnimator->EnableGlowEffect(enable);
        }
    }

    void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128) {
        if (mAnimator) {
            mAnimator->SetGlowColor(r, g, b, a);
        }
    }

    void EnableOverlayEffect(bool enable) {
        if (mAnimator) {
            mAnimator->EnableOverlayEffect(enable);
        }
    }

    void SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 64) {
        if (mAnimator) {
            mAnimator->SetOverlayColor(r, g, b, a);
        }
    }

    bool PlayTrack(const std::string& trackName, float blendTime = 0) {
        return mAnimator ? mAnimator->PlayTrack(trackName, blendTime) : false;
    }

    bool PlayTrackOnce(const std::string& trackName, const std::string& returnTrack = "", float speed = 1.0f, float blendTime = 0) {
        return mAnimator ? mAnimator->PlayTrackOnce(trackName, returnTrack, speed, blendTime) : false;
    }

};

#endif
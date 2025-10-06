#pragma once
#ifndef _REANIMATION_COMPONENT_H
#define _REANIMATION_COMPONENT_H

#include "ResourceManager.h"
#include "Component.h"
#include "Reanimator.h"
#include "AnimationTypes.h"
#include <memory>

class ReanimationComponent : public Component {
private:
    std::unique_ptr<Reanimation> mReanimation;
    AnimationType mAnimType;
    Vector mPosition;
    float mScale;
    bool mIsPlaying;
    ReanimLoopType mLoopType;

public:
    ReanimationComponent(AnimationType animType, const Vector& position = Vector::zero(), float scale = 1.0f)
        : mAnimType(animType), mPosition(position), mScale(scale), mIsPlaying(false),
        mLoopType(ReanimLoopType::REANIM_LOOP) {
    }

    ~ReanimationComponent() override {
        if (mReanimation) {
            mReanimation->ReanimationDie();
        }
    }

    void Start() override {
        auto gameObj = GetGameObject();
        if (!gameObj) return;

        SDL_Renderer* renderer = gameObj->GetRenderer();
        if (!renderer) {
			std::cout << 
                "ReanimationComponent::Start failed: Renderer is null" << std::endl;
            return;
        }
        mReanimation = std::make_unique<Reanimation>(renderer);

        ResourceManager& resMgr = ResourceManager::GetInstance();
        auto animDef = resMgr.GetAnimation(mAnimType);

        if (animDef) {
            mReanimation->ReanimationInitialize(mPosition.x, mPosition.y, animDef);
            mReanimation->SetScale(mScale);
            mReanimation->SetLoopType(mLoopType);

            if (mIsPlaying) {
                Play();
            }
        }
    }

    void Update() override {
        if (mReanimation && mIsPlaying) {
            mReanimation->Update();
        }
    }

    void Draw(SDL_Renderer* renderer) override {
        if (mReanimation && mIsPlaying) {
            mReanimation->Draw();
        }
    }

    void Play() {
        mIsPlaying = true;
        if (mReanimation != nullptr && mReanimation->IsDead())
        {
            mReanimation->ReanimationReset(); // ֻ���ѽ����Ķ�������
        }
    }

    void Stop() {
        mIsPlaying = false;
    }

    void SetLoopType(ReanimLoopType loopType) {
        mLoopType = loopType;
        if (mReanimation) {
            mReanimation->SetLoopType(loopType);
        }
    }

    void SetPosition(const Vector& position) {
        mPosition = position;
        if (mReanimation) {
            mReanimation->SetPosition(position.x, position.y);
        }
    }

    void SetScale(float scale) {
        mScale = scale;
        if (mReanimation) {
            mReanimation->SetScale(scale);
        }
    }

    bool IsPlaying() const { return mIsPlaying; }
    bool IsFinished() const { return mReanimation ? mReanimation->IsDead() : false; }

    // �����ض������֡
    void SetFramesForLayer(const std::string& trackName) {
        if (mReanimation) {
            mReanimation->SetFramesForLayer(trackName.c_str());
        }
    }
    // ���ҹ������
    int FindTrackIndex(const std::string& trackName) {
        return mReanimation ? mReanimation->FindTrackIndex(trackName.c_str()) : -1;
    }
	// ��ȡ�ײ�Reanimation*����
    Reanimation* GetReanimation() const {
        return mReanimation.get();
    }
};

#endif
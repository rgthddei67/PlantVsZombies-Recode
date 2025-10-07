#pragma once
#ifndef _REANIMATION_COMPONENT_H
#define _REANIMATION_COMPONENT_H

#include "ResourceManager.h"
#include "./Board&Game/Component.h"
#include "./Reanimation/Reanimator.h"
#include "./Reanimation/AnimationTypes.h"
#include <memory>

class GameObjectManager;

class ReanimationComponent : public Component {
private:
    std::unique_ptr<Reanimation> mReanimation;
    AnimationType mAnimType;
    float mScale;
    bool mIsPlaying;
    ReanimLoopType mLoopType;
    bool mAutoDestroy;

public:
    ReanimationComponent(AnimationType animType, const Vector& position = Vector::zero(), float scale = 1.0f)
        : mAnimType(animType), mScale(scale), mIsPlaying(false),
        mLoopType(ReanimLoopType::REANIM_LOOP), mAutoDestroy(true) {
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
        Vector startPos = Vector::zero();
        if (auto transformComp = gameObj->GetComponent<TransformComponent>()) 
        {
            startPos = transformComp->position;
        }
        else
        {
            std::cout<< 
				"ReanimationComponent::Start warning: TransformComponent not found, using (0,0) as start position" << std::endl;
        }
        mReanimation = std::make_unique<Reanimation>(renderer);

        ResourceManager& resMgr = ResourceManager::GetInstance();
        auto animDef = resMgr.GetAnimation(mAnimType);

        if (animDef) {
            mReanimation->ReanimationInitialize(startPos.x, startPos.y, animDef);
            mReanimation->SetScale(mScale);
            mReanimation->SetLoopType(mLoopType);
            mReanimation->SetGameObject(gameObj);
            mReanimation->SetAutoDestroy(mAutoDestroy);
            if (mIsPlaying) {
                Play();
            }
        }
    }

    void Update() override {
        if (mReanimation) {
            if (auto gameObj = GetGameObject()) {
                if (auto transformComp = gameObj->GetComponent<TransformComponent>()) {
                    SetPosition(transformComp->position);
                }
            }
            mReanimation->Update();
			// ��ѡ: �Զ������߼�
            if (mAutoDestroy && mReanimation->IsDead()) {
                if (auto gameObj = GetGameObject()) {
                    GameObjectManager::GetInstance().DestroyGameObject(gameObj);
                }
            }
        }
    }

    void Draw(SDL_Renderer* renderer) override {
        if (mReanimation) {
            mReanimation->Draw();
        }
    }

    void Play() {
        mIsPlaying = true;
        if (mReanimation != nullptr && mReanimation->IsDead())
        {
            mReanimation->SetPlaying(true);
            mReanimation->ReanimationReset(); // ֻ���ѽ����Ķ�������
        }
    }

    void Stop() {
        mIsPlaying = false;
        mReanimation->SetPlaying(false);
    }

    void SetLoopType(ReanimLoopType loopType) {
        mLoopType = loopType;
        if (mReanimation) {
            mReanimation->SetLoopType(loopType);
        }
    }

    void SetPosition(const Vector& position) {
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

    // �����Զ�����
    void SetAutoDestroy(bool autoDestroy) {
        mAutoDestroy = autoDestroy;
        if (mReanimation) {
            mReanimation->SetAutoDestroy(autoDestroy);
        }
    }

    // ��ȡ�Զ�����״̬
    bool GetAutoDestroy() const {
        return mAutoDestroy;
    }

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
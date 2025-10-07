#pragma once
#ifndef _REANIMATOR_H
#define _REANIMATOR_H
#include "AnimationTypes.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <memory>
#include "../ConstEnums.h"
#include "../Game/Definit.h"

// ǰ������
class ReanimatorDefinition;
class ReanimParser;

// �����任�ṹ
struct ReanimatorTransform 
{
    float mTransX, mTransY;    // λ��
    float mSkewX, mSkewY;      // ��ת����б��
    float mScaleX, mScaleY;    // ����
    float mFrame;              // ֡����
    float mAlpha;              // ͸����
    SDL_Texture* mImage;       // ����
	std::string mImageName;    // ͼƬ����

    ReanimatorTransform();
};

// �������
struct ReanimatorTrack {
    std::string mName;                            // �������
    std::vector<ReanimatorTransform> mTransforms; // �任����
    int mTransformCount;                          // �任����

    ReanimatorTrack();
};

// ��������
class ReanimatorDefinition {
public:
    std::vector<ReanimatorTrack> mTracks;  // �������
    float mFPS;                            // ֡��
    std::string mReanimFileName;           // �ļ���

	ReanimatorDefinition();
	~ReanimatorDefinition(); 

	bool LoadFromFile(const std::string& filename); // ���ļ����ض���
	bool LoadImages(SDL_Renderer* renderer);    // ��������ͼƬ
    void Clear();
};

class GameObject;

// ����ʵ��
class Reanimation 
{
private:
    std::shared_ptr<ReanimatorDefinition> mDefinition;     // ��������
    float mAnimTime;                       // ����ʱ�䣨0-1��
    float mAnimRate;                       // �����ٶ�
    ReanimLoopType mLoopType;              // ѭ������
    bool mDead;                            // �Ƿ��ѽ���
	bool mIsPlaying;    	               // �Ƿ��ڲ���
    int mFrameStart;                       // ��ʼ֡
    int mFrameCount;                       // ��֡��
    SexyTransform2D mOverlayMatrix;        // �任����
    int mLoopCount;                        // ѭ������
    bool mIsAttachment;                    // �Ƿ��Ǹ���
    int mRenderOrder;                      // ��Ⱦ˳��
    float mLastFrameTime;                  // ��һ֡ʱ��
    SDL_Renderer* mRenderer;               // SDL��Ⱦ��
    float mTotalDuration;                  // ������ʱ�����룩
    float mCurrentTime;                    // ��ǰ����ʱ�䣨��)
    Uint32 mLastUpdateTime;
    bool mFirstUpdate;
    float mScale = 1.0f;                   // ��С
    bool mAutoDestroy = false;
    std::weak_ptr<GameObject> mGameObjectWeak;

    struct ReanimatorFrameTime {
        float mFraction;
        int mAnimFrameBeforeInt;
        int mAnimFrameAfterInt;
    };

    void GetFrameTime(ReanimatorFrameTime* theFrameTime);
    void GetTransformAtTime(int theTrackIndex, ReanimatorTransform* theTransform, ReanimatorFrameTime* theFrameTime);

public:
    Reanimation(SDL_Renderer* renderer = nullptr);
    ~Reanimation();

    // ��ʼ������
    bool LoadReanimation(const std::string& reanimFile);
    void ReanimationInitialize(float x, float y, std::shared_ptr<ReanimatorDefinition> definition);

    // ���Ʒ���
    void Update();
    void Draw();
    void SetPosition(float x, float y);
    void SetRate(float rate);
    void SetLoopType(ReanimLoopType loopType);
    void ReanimationDie() { mDead = true; }
    void ReanimationReset() {
        mDead = false;
        mCurrentTime = 0.0f;
        mAnimTime = 0.0f;
        mLoopCount = 0;
        mFirstUpdate = true;
    }

    // ���Է���
    bool IsDead() const { return mDead; }
	bool IsPlaying() const { return mIsPlaying; }
    float GetAnimTime() const { return mAnimTime; }

    // �������
    int FindTrackIndex(const char* trackName);
    void SetFramesForLayer(const char* trackName);

    void SetScale(float scale) { mScale = scale; }
    float GetScale() const { return mScale; }
	void SetPlaying(bool isPlaying) { mIsPlaying = isPlaying; }
    void SetAutoDestroy(bool autoDestroy) { mAutoDestroy = autoDestroy; }
    bool GetAutoDestroy() const { return mAutoDestroy; }
    // ���ù�����GameObject
    void SetGameObject(std::shared_ptr<GameObject> gameObj) {
        mGameObjectWeak = gameObj;
    }
    // ��ȡ������GameObject
    std::shared_ptr<GameObject> GetGameObject() const {
        return mGameObjectWeak.lock();
    }
private:
    void DrawTrack(int trackIndex, int frameIndex);
};

#endif
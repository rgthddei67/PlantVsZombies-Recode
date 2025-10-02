#pragma once
#ifndef _REANIMATOR_H
#define _REANIMATOR_H
#include "AllCppInclude.h"
#include "ResourceManager.h"
#include <SDL.h>
#include <string>
#include <vector>
#include <memory>
#include "ConstEnums.h"
#include "Definit.h"

// ǰ������
class ReanimatorDefinition;
class ReanimParser;
enum class AnimationType;

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

// ����ʵ��
class Reanimation 
{
private:
    std::shared_ptr<ReanimatorDefinition> mDefinition;     // ��������
    float mAnimTime;                       // ����ʱ�䣨0-1��
    float mAnimRate;                       // �����ٶ�
    ReanimLoopType mLoopType;              // ѭ������
    bool mDead;                            // �Ƿ��ѽ���
    int mFrameStart;                       // ��ʼ֡
    int mFrameCount;                       // ��֡��
    SexyTransform2D mOverlayMatrix;        // �任����
    int mLoopCount;                        // ѭ������
    bool mIsAttachment;                    // �Ƿ��Ǹ���
    int mRenderOrder;                      // ��Ⱦ˳��
    float mLastFrameTime;                  // ��һ֡ʱ��
    SDL_Renderer* mRenderer;               // SDL��Ⱦ��
    float mTotalDuration;  // ������ʱ�����룩
    float mCurrentTime;    // ��ǰ����ʱ�䣨��)
    Uint32 mLastUpdateTime;
    bool mFirstUpdate;
    float mScale = 1.0f;                           // ��С

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

    // ���Է���
    bool IsDead() const { return mDead; }
    float GetAnimTime() const { return mAnimTime; }

    // �������
    int FindTrackIndex(const char* trackName);
    void SetFramesForLayer(const char* trackName);

    void SetScale(float scale) { mScale = scale; }
    float GetScale() const { return mScale; }

private:
    void DrawTrack(int trackIndex, int frameIndex);
};

// ����������
class ReanimationHolder 
{
private:
    std::vector<std::unique_ptr<Reanimation>> mReanimations;
    SDL_Renderer* mRenderer;

public:
    ReanimationHolder(SDL_Renderer* renderer = nullptr);
    ~ReanimationHolder();

    Reanimation* AllocReanimation(float x, float y, const std::string& reanimFile, float scale);
    Reanimation* AllocReanimation(float x, float y, ReanimatorDefinition* definition, float scale);
    Reanimation* AllocReanimation(float x, float y, AnimationType animType, float scale);
    void UpdateAll();
    void DrawAll();
    void ClearAll();
    size_t GetCount() const { return mReanimations.size(); }
};

#endif
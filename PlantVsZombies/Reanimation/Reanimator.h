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

// 前向声明
class ReanimatorDefinition;
class ReanimParser;

// 动画变换结构
struct ReanimatorTransform 
{
    float mTransX, mTransY;    // 位置
    float mSkewX, mSkewY;      // 旋转（倾斜）
    float mScaleX, mScaleY;    // 缩放
    float mFrame;              // 帧索引
    float mAlpha;              // 透明度
    SDL_Texture* mImage;       // 纹理
	std::string mImageName;    // 图片名称

    ReanimatorTransform();
};

// 动画轨道
struct ReanimatorTrack {
    std::string mName;                            // 轨道名称
    std::vector<ReanimatorTransform> mTransforms; // 变换数组
    int mTransformCount;                          // 变换数量

    ReanimatorTrack();
};

// 动画定义
class ReanimatorDefinition {
public:
    std::vector<ReanimatorTrack> mTracks;  // 轨道数组
    float mFPS;                            // 帧率
    std::string mReanimFileName;           // 文件名

	ReanimatorDefinition();
	~ReanimatorDefinition(); 

	bool LoadFromFile(const std::string& filename); // 从文件加载定义
	bool LoadImages(SDL_Renderer* renderer);    // 加载所有图片
    void Clear();
};

class GameObject;

// 动画实例
class Reanimation 
{
private:
    std::shared_ptr<ReanimatorDefinition> mDefinition;     // 动画定义
    float mAnimTime;                       // 动画时间（0-1）
    float mAnimRate;                       // 播放速度
    ReanimLoopType mLoopType;              // 循环类型
    bool mDead;                            // 是否已结束
	bool mIsPlaying;    	               // 是否在播放
    int mFrameStart;                       // 起始帧
    int mFrameCount;                       // 总帧数
    SexyTransform2D mOverlayMatrix;        // 变换矩阵
    int mLoopCount;                        // 循环次数
    bool mIsAttachment;                    // 是否是附件
    int mRenderOrder;                      // 渲染顺序
    float mLastFrameTime;                  // 上一帧时间
    SDL_Renderer* mRenderer;               // SDL渲染器
    float mTotalDuration;                  // 动画总时长（秒）
    float mCurrentTime;                    // 当前播放时间（秒)
    Uint32 mLastUpdateTime;
    bool mFirstUpdate;
    float mScale = 1.0f;                   // 大小
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

    // 初始化方法
    bool LoadReanimation(const std::string& reanimFile);
    void ReanimationInitialize(float x, float y, std::shared_ptr<ReanimatorDefinition> definition);

    // 控制方法
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

    // 属性访问
    bool IsDead() const { return mDead; }
	bool IsPlaying() const { return mIsPlaying; }
    float GetAnimTime() const { return mAnimTime; }

    // 轨道操作
    int FindTrackIndex(const char* trackName);
    void SetFramesForLayer(const char* trackName);

    void SetScale(float scale) { mScale = scale; }
    float GetScale() const { return mScale; }
	void SetPlaying(bool isPlaying) { mIsPlaying = isPlaying; }
    void SetAutoDestroy(bool autoDestroy) { mAutoDestroy = autoDestroy; }
    bool GetAutoDestroy() const { return mAutoDestroy; }
    // 设置关联的GameObject
    void SetGameObject(std::shared_ptr<GameObject> gameObj) {
        mGameObjectWeak = gameObj;
    }
    // 获取关联的GameObject
    std::shared_ptr<GameObject> GetGameObject() const {
        return mGameObjectWeak.lock();
    }
private:
    void DrawTrack(int trackIndex, int frameIndex);
};

#endif
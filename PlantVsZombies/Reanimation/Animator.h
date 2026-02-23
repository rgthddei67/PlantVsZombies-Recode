#pragma once
#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include "ReanimTypes.h"
#include "Reanimation.h"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include <iostream>

int ColorComponentMultiply(int theColor1, int theColor2);
SDL_Color ColorsMultiply(const SDL_Color& theColor1, const SDL_Color& theColor2);

struct AnimDrawCommand {
    SDL_Texture* texture;
    SDL_BlendMode blendMode;
    SDL_Color color;
    float points[8];  // 4个顶点的世界坐标 (x1,y1, x2,y2, x3,y3, x4,y4)
};

class Animator {
private:
    std::shared_ptr<Reanimation> mReanim;
    float mFPS = 12.0f;
    float mFrameIndexNow = 0.0f;
    float mFrameIndexBegin = 0.0f;
    float mFrameIndexEnd = 0.0f;
    float mSpeed = 1.0f;
    float mAlpha = 1.0f;

    // 过渡动画
    float mReanimBlendCounter = -1.0f;
    float mReanimBlendCounterMax = 100.0f;
    int mFrameIndexBlendBuffer = 0;

    bool mIsPlaying = false;
    PlayState mPlayingState = PlayState::PLAY_NONE;
    std::vector<TrackExtraInfo> mExtraInfos;
    std::unordered_map<std::string, int> mTrackIndicesMap;

    bool mEnableExtraAdditiveDraw = false;  // 高亮效果 
    bool mEnableExtraOverlayDraw = false;   // 附加覆盖效果
    SDL_Color mExtraAdditiveColor = { 255, 255, 255, 128 };
    SDL_Color mExtraOverlayColor = { 255, 255, 255, 64 };

    // 过渡目标
    std::string mTargetTrack = "";
    float mOriginalSpeed = 1.0f;    // 原来的速度

    std::unordered_multimap<int, std::function<void()>> mFrameEvents;  // 帧事件表

public:
    Animator();
    Animator(std::shared_ptr<Reanimation> reanim);
    ~Animator();

    // 初始化
    void Init(std::shared_ptr<Reanimation> reanim);

    void Die();

    // 播放控制
    void Play(PlayState state = PlayState::PLAY_REPEAT);
    void Pause();
    void Stop();

    // 增加帧事件
    void AddFrameEvent(int frameIndex, std::function<void()> callback);

    // 播放指定轨道动画，支持过渡效果
    bool PlayTrack(const std::string& trackName, float speed = 1.0f, float blendTime = 0);

    // 播放指定轨道动画一次，播放完后可切换回指定轨道
    bool PlayTrackOnce(const std::string& trackName,
        const std::string& returnTrack = "",
        float speed = 1.0f,
        float blendTime = 0);

    void SetOriginalSpeed(float speed) {
        this->mOriginalSpeed = speed;
    }

    float GetOriginalSpeed()
    {
        return this->mOriginalSpeed;
    }

    // 轨道范围控制
    std::pair<int, int> GetTrackRange(const std::string& trackName);
    void SetFrameRange(int frameBegin, int frameEnd);
    void SetFrameRangeByTrackName(const std::string& trackName);
    void SetFrameRangeToDefault();

    // 设置轨道自定义纹理（会覆盖动画原本的纹理）
    void SetTrackImage(const std::string& trackName, SDL_Texture* image);   
    void SetTrackVisible(const std::string& trackName, bool visible); // 轨道显示控制
 
    // 将子动画附加到指定轨道（跟随轨道变换）
    bool AttachAnimator(const std::string& trackName, std::shared_ptr<Animator> child);
    void DetachAnimator(const std::string& trackName, std::shared_ptr<Animator> child);
    void DetachAllAnimators();

    // 状态查询
    bool IsPlaying() const { return mIsPlaying; }
    float GetCurrentFrame() const { return mFrameIndexNow; }
    void SetSpeed(float speed);
    float GetSpeed() const { return mSpeed; }

    // 获取Track的动量
    float GetTrackVelocity(const std::string& trackName) const;

    // 透明度和颜色控制
    void SetAlpha(float alpha);
    float GetAlpha() const { return mAlpha; }

    void EnableGlowEffect(bool enable);
    void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128);

    void EnableOverlayEffect(bool enable);
    void SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 64);

    // 更新和渲染
    void Update();
    void Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale = 1.0f);

    // 获取底层Reanimation
    std::shared_ptr<Reanimation> GetReanimation() const { return mReanim; }

    // 获取轨道信息
    std::vector<TrackInfo*> GetTracksByName(const std::string& trackName) const;
    Vector GetTrackPosition(const std::string& trackName) const;
    float GetTrackRotation(const std::string& trackName) const;
    bool GetTrackVisible(const std::string& trackName) const;

    void SetLocalPosition(float x, float y);
    void SetLocalPosition(const Vector& position);
    void SetLocalScale(float sx, float sy);
    void SetLocalRotation(float rotation);

private:
    // 子动画相对于父轨道的本地变换（单位：像素、缩放因子、角度）
    float mLocalPosX = 0.0f;
    float mLocalPosY = 0.0f;
    float mLocalScaleX = 1.0f;
    float mLocalScaleY = 1.0f;
    float mLocalRotation = 0.0f;   // 角度制

private:
    TrackFrameTransform GetInterpolatedTransform(int trackIndex) const;
    std::vector<TrackExtraInfo*> GetTrackExtrasByName(const std::string& trackName);
    int GetFirstTrackIndexByName(const std::string& trackName) const;
    void CollectDrawCommands(std::vector<AnimDrawCommand>& outCommands, float baseX, float baseY, float Scale) const;
};

#endif
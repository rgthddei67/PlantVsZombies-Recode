#pragma once
#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include "ReanimTypes.h"
#include "Reanimation.h"
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>
#include <iostream>

int ColorComponentMultiply(int theColor1, int theColor2);

SDL_Color ColorsMultiply(const SDL_Color& theColor1, const SDL_Color& theColor2);

class Animator {
private:
    std::shared_ptr<Reanimation> mReanim;
    float mFPS = 12.0f;
    float mFrameIndexNow = 0.0f;
    float mFrameIndexBegin = 0.0f;
    float mFrameIndexEnd = 0.0f;
    float mSpeed = 1.0f;
    float mDeltaRate = 0.0f;
    float mAlpha = 1.0f;

    // 过渡动画
    float mReanimBlendCounter = -1.0f;
    float mReanimBlendCounterMax = 100.0f;
    int mFrameIndexBlendBuffer = 0;

    bool mIsPlaying = false;
    PlayState mPlayingState = PlayState::PLAY_NONE;
    std::vector<TrackExtraInfo> mExtraInfos;
    std::unordered_map<std::string, int> mTrackIndicesMap;
    std::string mTargetTrack;

    bool mEnableExtraAdditiveDraw = false;  // 是否启用附加叠加绘制
    bool mEnableExtraOverlayDraw = false;   // 是否启用覆盖叠加绘制
    SDL_Color mExtraAdditiveColor = { 255, 255, 255, 255 };  // 附加叠加颜色（默认半透明白色）
    SDL_Color mExtraOverlayColor = { 255, 255, 255, 255 };    // 覆盖叠加颜色

public:
    Animator();
    Animator(std::shared_ptr<Reanimation> reanim);
    ~Animator() = default;

    // 初始化
    void Init(std::shared_ptr<Reanimation> reanim);
    // 开始播放
    void Play(PlayState state = PlayState::PLAY_REPEAT);
    // 在当前播放位置暂停播放
    void Pause();
    // 完全停止播放并切换到第一帧
    void Stop();

    void EnableGlowEffect(bool enable) { mEnableExtraAdditiveDraw = enable; }

    void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128) {
        mExtraAdditiveColor = { r, g, b, a };
    }

    void EnableOverlayEffect(bool enable) { mEnableExtraOverlayDraw = enable; }

    void SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 64) {
        mExtraOverlayColor = { r, g, b, a };
    }

    void Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale);

    void Update();

    // 获取底层shared_ptr Reanimation
    std::shared_ptr<Reanimation> GetReanimation() const { return mReanim; }

    void SetAlpha(float alpha) { mAlpha = std::clamp(alpha, 0.0f, 1.0f); }
    float GetAlpha() const { return mAlpha; }

    // 帧范围控制
    std::pair<int, int> GetTrackRange(const std::string& trackName);
    void SetFrameRange(int frameBegin, int frameEnd);
    void SetFrameRangeByTrackName(const std::string& trackName);
    void SetFrameRangeToDefault();
    void SetFrameRangeByTrackNameOnce(const std::string& trackName, const std::string& oriTrackName);

    // 轨道控制
    void SetTrackVisibleByTrackName(const std::string& trackName, bool visible);
    void SetTrackVisible(int index, bool visible);
    void TrackAttachImageByTrackName(const std::string& trackName, SDL_Texture* target);
    void TrackAttachImage(int index, SDL_Texture* target);
    void TrackAttachAnimator(const std::string& trackName, Animator* target);
    void TrackAttachAnimatorMatrix(const std::string& trackName, glm::mat4* target);
    void TrackAttachOffsetByTrackName(const std::string& trackName, float offsetX, float offsetY);
    void TrackAttachOffset(int index, float offsetX, float offsetY);
    void TrackAttachFlashSpot(int index, float spot);
    void TrackAttachFlashSpotByTrackName(const std::string& trackName, float spot);

    // 查询功能
    float GetTrackVelocity(const std::string& trackName);
    bool GetTrackVisible(const std::string& trackName);
    std::vector<TrackExtraInfo*> GetAllTracksExtraByName(const std::string& trackName);
    std::vector<TrackInfo*> GetAllTracksByName(const std::string& trackName);
    int GetFirstTrackExtraIndexByName(const std::string& trackName);
    Vector GetTrackPos(const std::string& trackname);
    float GetTrackRotate(const std::string& trackname);

    // 状态查询
    bool IsPlaying() const { return mIsPlaying; }
    float GetCurrentFrame() const { return mFrameIndexNow; }
    void SetSpeed(float speed) { mSpeed = speed; }
    float GetSpeed() const { return mSpeed; }

private:
    TrackFrameTransform GetInterpolatedTransform(int trackIndex) const;
    glm::mat4 CalculateTransformMatrix(const TrackFrameTransform& tf, const TrackExtraInfo& extra, float Scale) const;
};

#endif
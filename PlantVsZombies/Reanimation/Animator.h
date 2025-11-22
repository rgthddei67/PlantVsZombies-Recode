#pragma once
#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include "ReanimTypes.h"
#include "Reanimation.h"
#include <unordered_map>
#include <memory>

class Animator {
private:
    std::shared_ptr<Reanimation> mReanim;
    float mFPS = 12.0f;
    float mFrameIndexNow = 0.0f;
    float mFrameIndexBegin = 0.0f;
    float mFrameIndexEnd = 0.0f;
    float mSpeed = 1.0f;
    float mDeltaRate = 0.0f;

    // 过渡动画
    float mReanimBlendCounter = -1.0f;
    float mReanimBlendCounterMax = 100.0f;
    int mFrameIndexBlendBuffer = 0;

    bool mIsPlaying = false;
    PlayState mPlayingState = PlayState::PLAY_NONE;
    std::vector<TrackExtraInfo> mExtraInfos;
    std::unordered_map<std::string, int> mTrackIndicesMap;
    std::string mTargetTrack;

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

    void Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale);
    void Update();

    // 获取底层shared_ptr Reanimation
    std::shared_ptr<Reanimation> GetReanimation() const { return mReanim; }

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
    void TrackAttachAnimatorMatrix(const std::string& trackName, SexyTransform2D* target);
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
};

#endif
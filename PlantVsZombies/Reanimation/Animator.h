#pragma once
#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include "ReanimTypes.h"
#include "Reanimation.h"
#include <algorithm>
#include <unordered_map>
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include <iostream>

int ColorComponentMultiply(int theColor1, int theColor2);
SDL_Color ColorsMultiply(const SDL_Color& theColor1, const SDL_Color& theColor2);

class Animator {
private:
    std::shared_ptr<Reanimation> mReanim;
    AnimationClip mCurrentClip;
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

    std::string mCurrentAnimationName;
    bool mAutoSwitchAnimation = true;
    std::map<std::string, AnimationClip> mAnimationClips;
        
    bool mEnableExtraAdditiveDraw = false;  // 高亮效果 
	bool mEnableExtraOverlayDraw = false;   // 附加覆盖效果
    SDL_Color mExtraAdditiveColor = { 255, 255, 255, 128 };
    SDL_Color mExtraOverlayColor = { 255, 255, 255, 64 };

    // 过渡目标
    std::string mTargetTrack;

public:
    Animator();
    Animator(std::shared_ptr<Reanimation> reanim);
    ~Animator() = default;

    // 初始化
    void Init(std::shared_ptr<Reanimation> reanim);

    // 播放控制
    void Play(PlayState state = PlayState::PLAY_REPEAT);
    void Pause();
    void Stop();

    // 播放指定轨道动画，支持过渡效果
    bool PlayTrack(const std::string& trackName, int blendTime = 0);

	// 播放指定轨道动画一次，播放完后可切换回指定轨道
    bool PlayTrackOnce(const std::string& trackName, const std::string& returnTrack = "", int blendTime = 0);

    bool SetAnimation(const std::string& animationName);
    bool SetAnimation(const std::string& trackName, const std::string& clipName);

    // 获取动画信息
    std::string GetCurrentAnimationName() const { return mCurrentAnimationName; }
    std::vector<std::string> GetAvailableAnimations() const;

    // 轨道范围控制
    std::pair<int, int> GetTrackRange(const std::string& trackName);
    void SetFrameRange(int frameBegin, int frameEnd);
    void SetFrameRangeByTrackName(const std::string& trackName);
    void SetFrameRangeToDefault();

    // 轨道显示控制
    void SetTrackVisible(const std::string& trackName, bool visible);
    void SetTrackImage(const std::string& trackName, SDL_Texture* image);
    void SetTrackOffset(const std::string& trackName, float offsetX, float offsetY);
    void AttachAnimator(const std::string& trackName, Animator* animator);

    // 状态查询
    bool IsPlaying() const { return mIsPlaying; }
    float GetCurrentFrame() const { return mFrameIndexNow; }
    void SetSpeed(float speed) { mSpeed = speed; }
    float GetSpeed() const { return mSpeed; }

    // 透明度和颜色控制
    void SetAlpha(float alpha) { mAlpha = std::clamp(alpha, 0.0f, 1.0f); }
    float GetAlpha() const { return mAlpha; }

    void EnableGlowEffect(bool enable) { mEnableExtraAdditiveDraw = enable; }
    void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128) {
        mExtraAdditiveColor = { r, g, b, a };
    }

    void EnableOverlayEffect(bool enable) { mEnableExtraOverlayDraw = enable; }
    void SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 64) {
        mExtraOverlayColor = { r, g, b, a };
    }

    // 更新和渲染
    void Update();
    void Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale = 1.0f);

    // 获取底层Reanimation
    std::shared_ptr<Reanimation> GetReanimation() const { return mReanim; }

    // 获取轨道信息
    std::vector<TrackInfo*> GetTracksByName(const std::string& trackName);
    Vector GetTrackPosition(const std::string& trackName);
    float GetTrackRotation(const std::string& trackName);
    bool GetTrackVisible(const std::string& trackName);

    // 设置自动切换
    void SetAutoSwitch(bool autoSwitch) { mAutoSwitchAnimation = autoSwitch; }

private:
    TrackFrameTransform GetInterpolatedTransform(int trackIndex) const;
    std::vector<TrackExtraInfo*> GetTrackExtrasByName(const std::string& trackName);
    int GetFirstTrackIndexByName(const std::string& trackName);
};

#endif
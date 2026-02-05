#pragma once
#ifndef _REANIM_TYPES_H
#define _REANIM_TYPES_H

#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "../Game/Definit.h"

// 动画帧变换数据
struct TrackFrameTransform {
    float x = 0.0f;
    float y = 0.0f;
    float kx = 0.0f;  // 旋转X
    float ky = 0.0f;  // 旋转Y  
    float sx = 1.0f;  // 缩放X
    float sy = 1.0f;  // 缩放Y
    float a = 1.0f;   // 透明度
    int f = 0;        // 显示标志（0=显示，-1=空白/分隔）
    std::string image; // 图像ID

    TrackFrameTransform() = default;
};

struct AnimationClip {
    std::string trackName;  // 轨道名称
    std::string clipName;   // 片段名称
    int startFrame;         // 起始帧
    int endFrame;           // 结束帧
    int totalFrames;        // 总帧数

    AnimationClip() : startFrame(0), endFrame(0), totalFrames(0) {}
    AnimationClip(const std::string& track, int start, int end)
        : trackName(track), startFrame(start), endFrame(end),
        totalFrames(end - start + 1) {
    }

    bool IsValid() const { return totalFrames > 0; }
    bool ContainsFrame(int frame) const {
        return frame >= startFrame && frame <= endFrame;
    }
};

// 动画轨道信息
struct TrackInfo {
    std::string mTrackName = "";
    bool mAvailable = true;
    std::vector<TrackFrameTransform> mFrames;

    // 动画片段信息
    std::vector<AnimationClip> mClips;
    AnimationClip mDefaultClip;

    TrackInfo() = default;
    explicit TrackInfo(const std::string& name) : mTrackName(name) {}

    AnimationClip* GetClip(const std::string& clipName = "") {
        if (clipName.empty()) {
            return mDefaultClip.IsValid() ? &mDefaultClip :
                (mClips.empty() ? nullptr : &mClips[0]);
        }

        for (auto& clip : mClips) {
            if (clip.clipName == clipName) {
                return &clip;
            }
        }
        return nullptr;
    }
};

// 轨道额外控制信息
struct TrackExtraInfo {
    bool mVisible = true;
    class Animator* mAttachedReanim = nullptr;
    SDL_Texture* mAttachedImage = nullptr;
    float mOffsetX = 0.0f;
    float mOffsetY = 0.0f;

    TrackExtraInfo() = default;
};

// 动画播放状态
enum class PlayState {
    PLAY_NONE,
    PLAY_REPEAT,
    PLAY_ONCE,
    PLAY_ONCE_TO,
};

#endif
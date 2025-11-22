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
    int f = 0;        // 是否隐藏 (0=显示, 1=隐藏)
    std::string i;    // 图像ID

    TrackFrameTransform() = default;
};

// 动画轨道信息
struct TrackInfo {
    std::string mTrackName = "";
    bool mAvailable = true;
    std::vector<TrackFrameTransform> mFrames;

    TrackInfo() = default;
    explicit TrackInfo(const std::string& name) : mTrackName(name) {}
};

// 轨道额外控制信息
struct TrackExtraInfo {
    bool mVisible = true;
    class Animator* mAttachedReanim = nullptr;
    SDL_Texture* mAttachedImage = nullptr;
    SexyTransform2D* mAttachedReanimMatrix = nullptr;
    float mOffsetX = 0.0f;
    float mOffsetY = 0.0f;
    float mFlashSpotSingle = 0.0f;

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
#pragma once
#ifndef _REANIM_TYPES_H
#define _REANIM_TYPES_H

#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include <memory>
#include <unordered_map>
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
    SDL_Texture* image = nullptr; // 图像

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
    float mOffsetX = 0.0f;          // 轨道自身绘制偏移 X
    float mOffsetY = 0.0f;          // 轨道自身绘制偏移 Y
    SDL_Texture* mImage = nullptr;  // 手动覆盖图片设置
    std::unordered_map<std::string, SDL_Texture*> mTextureCache; // 图片名 -> 纹理
    std::vector<std::weak_ptr<class Animator>> mAttachedReanims;  // 附加的子动画
};

// 动画播放状态
enum class PlayState {
    PLAY_NONE,
    PLAY_REPEAT,
    PLAY_ONCE,
    PLAY_ONCE_TO,
};

#endif
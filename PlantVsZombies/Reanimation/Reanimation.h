#pragma once
#ifndef _REANIMATION_H
#define _REANIMATION_H

#include "ReanimTypes.h"
#include <memory>
#include <vector>
#include <set>

constexpr float REANIM_MISSING_FIELD_FLOAT = -1024;
constexpr int REANIM_MISSING_FIELD_INT = -1024;

class Reanimation {
public:
    float mFPS = 12.0f;
    std::shared_ptr<std::vector<TrackInfo>> mTracks = nullptr;
    bool mIsLoaded = false;
    class ResourceManager* mResourceManager = nullptr;

public:
    Reanimation();
    ~Reanimation();

    // 加载reanim文件
    bool LoadFromFile(const std::string& filePath);

    // 获取轨道信息
    size_t GetTrackCount() const;
    TrackInfo* GetTrack(int index);
    TrackInfo* GetTrack(const std::string& trackName);

    // 获取总帧数
    int GetTotalFrames() const;
};

void GetDeltaTransform(const TrackFrameTransform& tSrc, const TrackFrameTransform& tDst,
    float tDelta, TrackFrameTransform& tOutput, bool useDestFrame = false);

#endif
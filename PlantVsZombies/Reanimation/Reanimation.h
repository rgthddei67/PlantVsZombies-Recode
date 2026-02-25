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

    // 鍔犺浇reanim鏂囦欢
    bool LoadFromFile(const std::string& filePath);

    // 鑾峰彇杞ㄩ亾淇℃伅
    size_t GetTrackCount() const;
    TrackInfo* GetTrack(int index);
    TrackInfo* GetTrack(const std::string& trackName);

    // 鑾峰彇鎬诲抚鏁?
    int GetTotalFrames() const;
};

void GetDeltaTransform(const TrackFrameTransform& tSrc, const TrackFrameTransform& tDst,
    float tDelta, TrackFrameTransform& tOutput, bool useDestFrame = false);

#endif
#pragma once
#ifndef _REANIMATION_H
#define _REANIMATION_H

#include "ReanimTypes.h"
#include <memory>
#include <vector>
#include <set>

class Reanimation {
public:
    float mFPS = 12.0f;
    std::shared_ptr<std::vector<TrackInfo>> mTracks = nullptr;
    std::shared_ptr<std::set<std::string>> mImagesSet = nullptr;
    bool mIsLoaded = false;
    class ResourceManager* mResourceManager = nullptr;

public:
    Reanimation();
    ~Reanimation();

    // 从文件加载动画
    bool LoadFromFile(const std::string& filePath);

    // 获取轨道数量
    int GetTrackCount() const { return mTracks ? mTracks->size() : 0; }

    // 查找轨道索引
    int FindTrackIndex(const std::string& trackName) const;

    // 获取轨道信息
    TrackInfo* GetTrack(int index);
    TrackInfo* GetTrack(const std::string& trackName);

    // 获取轨道范围
    std::pair<int, int> GetTrackFrameRange(const std::string& trackName) const;

    // 获取总帧数
    int GetTotalFrames() const;
};

// 矩阵变换工具函数
void TransformToMatrix(const TrackFrameTransform& src, SexyTransform2D& dest,
    float scaleX = 1.0f, float scaleY = 1.0f,
    float offsetX = 0.0f, float offsetY = 0.0f);

// 帧插值计算
void GetDeltaTransform(const TrackFrameTransform& tSrc, const TrackFrameTransform& tDst,
    float tDelta, TrackFrameTransform& tOutput, bool useDestFrame = false);

#endif
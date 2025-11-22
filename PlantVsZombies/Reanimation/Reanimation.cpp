#include "Reanimation.h"
#include "../ResourceManager.h"
#include <iostream>
#include <pugixml.hpp>

Reanimation::Reanimation() {
    mTracks = std::make_shared<std::vector<TrackInfo>>();
    mImagesSet = std::make_shared<std::set<std::string>>();
}

Reanimation::~Reanimation() {
    // 自动清理
}

bool Reanimation::LoadFromFile(const std::string& filePath) {
    // 清空现有数据
    mTracks->clear();
    mImagesSet->clear();
    mIsLoaded = false;

    // 解析.reanim文件
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filePath.c_str());

    if (!result) {
        std::cerr << "Failed to load reanim file: " << filePath
            << " - " << result.description() << std::endl;
        return false;
    }

    // 遍历XML文档
    for (pugi::xml_node node : doc.children()) {
        std::string tagName = node.name();
        if (tagName == "fps") {
            mFPS = node.text().as_float();
        }
        else if (tagName == "track") {
            TrackInfo track;
            int frameCount = 0;

            for (pugi::xml_node child : node.children()) {
                std::string childName = child.name();
                if (childName == "name") {
                    track.mTrackName = child.text().as_string();
                }
                else if (childName == "t") {
                    TrackFrameTransform transform;

                    // 初始化transform，使用前一帧的值（如果存在）作为默认值
                    if (frameCount > 0 && !track.mFrames.empty()) {
                        transform = track.mFrames[frameCount - 1];
                    }
                    else {
                        transform = TrackFrameTransform(); // 使用默认构造函数
                    }

                    // 解析变换属性
                    for (pugi::xml_node prop : child.children()) {
                        std::string propName = prop.name();
                        if (propName == "x") {
                            transform.x = prop.text().as_float();
                        }
                        else if (propName == "y") {
                            transform.y = prop.text().as_float();
                        }
                        else if (propName == "kx") {
                            transform.kx = prop.text().as_float();
                        }
                        else if (propName == "ky") {
                            transform.ky = prop.text().as_float();
                        }
                        else if (propName == "sx") {
                            transform.sx = prop.text().as_float();
                        }
                        else if (propName == "sy") {
                            transform.sy = prop.text().as_float();
                        }
                        else if (propName == "a") {
                            transform.a = prop.text().as_float();
                        }
                        else if (propName == "f") {
                            transform.f = prop.text().as_int();
                        }
                        // TODO 动画图片处理
                        else if (propName == "i") {
                            transform.i = prop.text().as_string();
                            mImagesSet->insert(transform.i);
                            if (mResourceManager && !transform.i.empty()) {
                                // 检查是否是 REANIM 图片格式
                                if (transform.i.find("IMAGE_REANIM_") == 0) {
                                    // 提取实际文件名：去掉 "IMAGE_REANIM_" 前缀
                                    std::string fileName = transform.i.substr(13);

                                    std::string filePath = "./resources/image/reanim/" + fileName;

                                    SDL_Texture* texture = mResourceManager->LoadTexture(filePath + ".png", transform.i);
                                    if (!texture) {
                                        texture = mResourceManager->LoadTexture(filePath + ".jpg", transform.i);
                                    }
#ifdef _DEBUG
                                    if (texture) {
                                        std::cout << "成功加载动画图片: " << transform.i << " -> " << filePath << std::endl;
                                    }
                                    else {
                                        std::cout << "警告: 无法加载动画图片: " << transform.i << std::endl;
                                    }
#endif
                                }
                            }
                        }
                    }

                    track.mFrames.push_back(transform);
                    frameCount++;
                }
            }

            mTracks->push_back(track);
        }
    }

    mIsLoaded = true;
    std::cout << "Successfully loaded reanim: " << filePath
        << " with " << mTracks->size() << " tracks and "
        << GetTotalFrames() << " frames" << std::endl;
    return true;
}

int Reanimation::FindTrackIndex(const std::string& trackName) const {
    if (!mTracks) return -1;

    for (size_t i = 0; i < mTracks->size(); i++) {
        if ((*mTracks)[i].mTrackName == trackName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

TrackInfo* Reanimation::GetTrack(int index) {
    if (!mTracks || index < 0 || index >= static_cast<int>(mTracks->size())) {
        return nullptr;
    }
    return &(*mTracks)[index];
}

TrackInfo* Reanimation::GetTrack(const std::string& trackName) {
    int index = FindTrackIndex(trackName);
    return index >= 0 ? GetTrack(index) : nullptr;
}

std::pair<int, int> Reanimation::GetTrackFrameRange(const std::string& trackName) const {
    auto track = const_cast<Reanimation*>(this)->GetTrack(trackName);
    if (!track || track->mFrames.empty()) {
        return { 0, 0 };
    }

    return { 0, static_cast<int>(track->mFrames.size() - 1) };
}

int Reanimation::GetTotalFrames() const {
    if (!mTracks || mTracks->empty()) return 0;
    return static_cast<int>((*mTracks)[0].mFrames.size());
}

void TransformToMatrix(const TrackFrameTransform& src, SexyTransform2D& dest,
    float scaleX, float scaleY, float offsetX, float offsetY) {

    // 先设置旋转和平移，然后缩放
    float radX = src.kx * -0.017453292f; // -π/180
    float radY = src.ky * -0.017453292f;

    // 直接设置矩阵元素（参考 GameMat44 的布局）
    // 假设 SexyTransform2D 的布局是：
    // [m00 m01 m02]
    // [m10 m11 m12]  
    // [m20 m21 m22]

    // 先设置旋转部分
    dest.m00 = cos(radX);
    dest.m01 = -sin(radX);
    dest.m02 = 0.0f;

    dest.m10 = sin(radY);
    dest.m11 = cos(radY);
    dest.m12 = 0.0f;

    dest.m20 = 0.0f;
    dest.m21 = 0.0f;
    dest.m22 = 1.0f;

    // 设置平移
    dest.m02 = src.x * scaleX + offsetX;
    dest.m12 = src.y * scaleY + offsetY;

    // 应用缩放（参考代码在最后应用缩放）
    dest.m00 *= src.sx * scaleX;
    dest.m01 *= src.sx * scaleX;
    dest.m10 *= src.sy * scaleY;
    dest.m11 *= src.sy * scaleY;
}

void GetDeltaTransform(const TrackFrameTransform& tSrc, const TrackFrameTransform& tDst,
    float tDelta, TrackFrameTransform& tOutput, bool useDestFrame) {

    // 基本属性插值
    tOutput.a = (tDst.a - tSrc.a) * tDelta + tSrc.a;
    tOutput.x = (tDst.x - tSrc.x) * tDelta + tSrc.x;
    tOutput.y = (tDst.y - tSrc.y) * tDelta + tSrc.y;
    tOutput.sx = (tDst.sx - tSrc.sx) * tDelta + tSrc.sx;
    tOutput.sy = (tDst.sy - tSrc.sy) * tDelta + tSrc.sy;

    // 角度插值（处理360度环绕）
    float angleDiffX = tDst.kx - tSrc.kx;
    float angleDiffY = tDst.ky - tSrc.ky;

    // 确保角度差在[-180, 180]范围内
    if (angleDiffX > 180.0f) angleDiffX -= 360.0f;
    if (angleDiffX < -180.0f) angleDiffX += 360.0f;
    if (angleDiffY > 180.0f) angleDiffY -= 360.0f;
    if (angleDiffY < -180.0f) angleDiffY += 360.0f;

    tOutput.kx = tSrc.kx + angleDiffX * tDelta;
    tOutput.ky = tSrc.ky + angleDiffY * tDelta;

    if (useDestFrame)
        tOutput.f = tDst.f;
    else
        tOutput.f = tSrc.f;

    tOutput.i = tSrc.i;
}
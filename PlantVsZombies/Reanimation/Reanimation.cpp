#include "Reanimation.h"
#include "../ResourceManager.h"
#include <iostream>
#include "../FileManager.h"
#include <glm/glm.hpp>

Reanimation::Reanimation() {
    mTracks = std::make_shared<std::vector<TrackInfo>>();
}

Reanimation::~Reanimation() {
    // 自动清理
}

bool Reanimation::LoadFromFile(const std::string& filePath) {
    // 清空现有数据
    mTracks->clear();
    mIsLoaded = false;

    // 解析.reanim文件
    pugi::xml_document doc;
    if (!FileManager::LoadXMLFile(filePath, doc)) {
        std::cerr << "Failed to load reanim file: " << filePath << std::endl;
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

            // 初始化前一帧的数据
            float prevX = 0.0f;
            float prevY = 0.0f;
            float prevKx = 0.0f;
            float prevKy = 0.0f;
            float prevSx = 1.0f;
            float prevSy = 1.0f;
            float prevA = 1.0f;
            int prevF = 0;
            const GLTexture* prevImage = nullptr;

            for (pugi::xml_node child : node.children()) {
                std::string childName = child.name();
                if (childName == "name") {
                    track.mTrackName = child.text().as_string();
                }
                else if (childName == "t") {
                    TrackFrameTransform frameTransform;

                    // 初始化所有字段为占位符值
                    frameTransform.x = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.y = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.kx = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.ky = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.sx = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.sy = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.a = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.f = REANIM_MISSING_FIELD_INT;
                    frameTransform.image = nullptr;

                    // 解析变换属性
                    for (pugi::xml_node prop : child.children()) {
                        std::string propName = prop.name();
                        std::string propValue = prop.text().as_string();

                        // 跳过空值
                        if (propValue.empty()) continue;

                        if (propName == "x") {
                            frameTransform.x = prop.text().as_float();
                        }
                        else if (propName == "y") {
                            frameTransform.y = prop.text().as_float();
                        }
                        else if (propName == "kx") {
                            frameTransform.kx = prop.text().as_float();
                        }
                        else if (propName == "ky") {
                            frameTransform.ky = prop.text().as_float();
                        }
                        else if (propName == "sx") {
                            frameTransform.sx = prop.text().as_float();
                        }
                        else if (propName == "sy") {
                            frameTransform.sy = prop.text().as_float();
                        }
                        else if (propName == "a") {
                            frameTransform.a = prop.text().as_float();
                        }
                        else if (propName == "f") {
                            frameTransform.f = prop.text().as_int();
                        }
                        else if (propName == "i") {
                            std::string imageName = prop.text().as_string();
                            if (!imageName.empty() && mResourceManager) {
                                // 检查是否是 REANIM 图片格式
                                if (imageName.find("IMAGE_REANIM_") == 0) {
                                    std::string fileName = imageName.substr(13);
                                    std::string filePath = "./resources/image/reanim/" + fileName;

                                    prevImage = mResourceManager->LoadTexture(filePath + ".png", imageName);
                                    if (!prevImage) {
                                        prevImage = mResourceManager->LoadTexture(filePath + ".jpg", imageName);
                                    }

                                    if (!prevImage) {
                                        std::cout << "警告: 无法加载动画图片: " << imageName << std::endl;
                                    }
                                    frameTransform.image = prevImage;
                                }
                            }
                         
                        }
                    }

                    // 如果当前帧的值是占位符，使用前一帧的值
                    if (frameTransform.x == REANIM_MISSING_FIELD_FLOAT)
                        frameTransform.x = prevX;
                    else
                        prevX = frameTransform.x;

                    if (frameTransform.y == REANIM_MISSING_FIELD_FLOAT)
                        frameTransform.y = prevY;
                    else
                        prevY = frameTransform.y;

                    if (frameTransform.kx == REANIM_MISSING_FIELD_FLOAT)
                        frameTransform.kx = prevKx;
                    else
                        prevKx = frameTransform.kx;

                    if (frameTransform.ky == REANIM_MISSING_FIELD_FLOAT)
                        frameTransform.ky = prevKy;
                    else
                        prevKy = frameTransform.ky;

                    if (frameTransform.sx == REANIM_MISSING_FIELD_FLOAT)
                        frameTransform.sx = prevSx;
                    else
                        prevSx = frameTransform.sx;

                    if (frameTransform.sy == REANIM_MISSING_FIELD_FLOAT)
                        frameTransform.sy = prevSy;
                    else
                        prevSy = frameTransform.sy;

                    if (frameTransform.a == REANIM_MISSING_FIELD_FLOAT)
                        frameTransform.a = prevA;
                    else
                        prevA = frameTransform.a;

                    if (frameTransform.f == REANIM_MISSING_FIELD_INT) frameTransform.f = prevF;
                    else prevF = frameTransform.f;

                    if (frameTransform.image == nullptr) 
                        frameTransform.image = prevImage; 
                    else 
                        prevImage = frameTransform.image;

                   // 添加帧到轨道
                    track.mFrames.push_back(frameTransform);
                }
            }

            // 轨道可用性
            track.mAvailable = !track.mFrames.empty();

            mTracks->push_back(track);
        }
    }

    mIsLoaded = true;

#ifdef _DEBUG
    std::cout << "成功加载reanim文件: " << filePath
        << "，轨道数: " << mTracks->size()
        << "，总帧数: " << GetTotalFrames() << std::endl;
#endif

    return true;
}

TrackInfo* Reanimation::GetTrack(int index) {
    if (!mTracks || index < 0 || index >= static_cast<int>(mTracks->size())) {
        return nullptr;
    }
    return &(*mTracks)[index];
}

TrackInfo* Reanimation::GetTrack(const std::string& trackName) {
    for (size_t i = 0; i < mTracks->size(); i++) {
        if ((*mTracks)[i].mTrackName == trackName) {
            return &(*mTracks)[i];
        }
    }
    return nullptr;
}

int Reanimation::GetTotalFrames() const {
    if (!mTracks || mTracks->empty()) return 0;
    return static_cast<int>((*mTracks)[0].mFrames.size());
}

size_t Reanimation::GetTrackCount() const {
    return mTracks ? mTracks->size() : 0;
}

void GetDeltaTransform(const TrackFrameTransform& tSrc, const TrackFrameTransform& tDst,
    float tDelta, TrackFrameTransform& tOutput, bool useDestFrame) {

    tOutput.a = (tDst.a - tSrc.a) * tDelta + tSrc.a;
    tOutput.x = (tDst.x - tSrc.x) * tDelta + tSrc.x;
    tOutput.y = (tDst.y - tSrc.y) * tDelta + tSrc.y;
    tOutput.sx = (tDst.sx - tSrc.sx) * tDelta + tSrc.sx;
    tOutput.sy = (tDst.sy - tSrc.sy) * tDelta + tSrc.sy;

    if (useDestFrame) {
        // 混合模式：角度差超过180°时，目标角度视为源角度
        float kxDst = tDst.kx;
        float kyDst = tDst.ky;
        if (kxDst > tSrc.kx + 180.0f || kxDst < tSrc.kx - 180.0f)
            kxDst = tSrc.kx;
        if (kyDst > tSrc.ky + 180.0f || kyDst < tSrc.ky - 180.0f)
            kyDst = tSrc.ky;
        tOutput.kx = (kxDst - tSrc.kx) * tDelta + tSrc.kx;
        tOutput.ky = (kyDst - tSrc.ky) * tDelta + tSrc.ky;
    }
    else {
        // 正常帧间插值：取最短旋转路径
        float kxDiff = tDst.kx - tSrc.kx;
        while (kxDiff > 180.0f) kxDiff -= 360.0f;
        while (kxDiff < -180.0f) kxDiff += 360.0f;
        tOutput.kx = tSrc.kx + kxDiff * tDelta;

        float kyDiff = tDst.ky - tSrc.ky;
        while (kyDiff > 180.0f) kyDiff -= 360.0f;
        while (kyDiff < -180.0f) kyDiff += 360.0f;
        tOutput.ky = tSrc.ky + kyDiff * tDelta;
    }

    if (useDestFrame)
        tOutput.f = tDst.f;
    else
        tOutput.f = tSrc.f;

    tOutput.image = tSrc.image;
}
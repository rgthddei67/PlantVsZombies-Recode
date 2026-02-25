#include "Reanimation.h"
#include "../ResourceManager.h"
#include <iostream>
#include "../FileManager.h"
#include <glm/glm.hpp>

Reanimation::Reanimation() {
    mTracks = std::make_shared<std::vector<TrackInfo>>();
}

Reanimation::~Reanimation() {
    // 鑷姩娓呯悊
}

bool Reanimation::LoadFromFile(const std::string& filePath) {
    // 娓呯┖鐜版湁鏁版嵁
    mTracks->clear();
    mIsLoaded = false;

    // 瑙ｆ瀽.reanim鏂囦欢
    pugi::xml_document doc;
    if (!FileManager::LoadXMLFile(filePath, doc)) {
        std::cerr << "Failed to load reanim file: " << filePath << std::endl;
        return false;
    }

    // 閬嶅巻XML鏂囨。
    for (pugi::xml_node node : doc.children()) {
        std::string tagName = node.name();
        if (tagName == "fps") {
            mFPS = node.text().as_float();
        }
        else if (tagName == "track") {
            TrackInfo track;

            // 鍒濆鍖栧墠涓€甯х殑鏁版嵁
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

                    // 鍒濆鍖栨墍鏈夊瓧娈典负鍗犱綅绗﹀€?
                    frameTransform.x = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.y = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.kx = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.ky = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.sx = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.sy = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.a = REANIM_MISSING_FIELD_FLOAT;
                    frameTransform.f = REANIM_MISSING_FIELD_INT;
                    frameTransform.image = nullptr;

                    // 瑙ｆ瀽鍙樻崲灞炴€?
                    for (pugi::xml_node prop : child.children()) {
                        std::string propName = prop.name();
                        std::string propValue = prop.text().as_string();

                        // 璺宠繃绌哄€?
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
                                // 妫€鏌ユ槸鍚︽槸 REANIM 鍥剧墖鏍煎紡
                                if (imageName.find("IMAGE_REANIM_") == 0) {
                                    std::string fileName = imageName.substr(13);
                                    std::string filePath = "./resources/image/reanim/" + fileName;

                                    prevImage = mResourceManager->LoadTexture(filePath + ".png", imageName);
                                    if (!prevImage) {
                                        prevImage = mResourceManager->LoadTexture(filePath + ".jpg", imageName);
                                    }

                                    if (!prevImage) {
                                        std::cout << "璀﹀憡: 鏃犳硶鍔犺浇鍔ㄧ敾鍥剧墖: " << imageName << std::endl;
                                    }
                                    frameTransform.image = prevImage;
                                }
                            }
                         
                        }
                    }

                    // 濡傛灉褰撳墠甯х殑鍊兼槸鍗犱綅绗︼紝浣跨敤鍓嶄竴甯х殑鍊?
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

                   // 娣诲姞甯у埌杞ㄩ亾
                    track.mFrames.push_back(frameTransform);
                }
            }

            // 杞ㄩ亾鍙敤鎬?
            track.mAvailable = !track.mFrames.empty();

            mTracks->push_back(track);
        }
    }

    mIsLoaded = true;

#ifdef _DEBUG
    std::cout << "鎴愬姛鍔犺浇reanim鏂囦欢: " << filePath
        << "锛岃建閬撴暟: " << mTracks->size()
        << "锛屾€诲抚鏁? " << GetTotalFrames() << std::endl;
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
        // 娣峰悎妯″紡锛氳搴﹀樊瓒呰繃180掳鏃讹紝鐩爣瑙掑害瑙嗕负婧愯搴?
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
        // 姝ｅ父甯ч棿鎻掑€硷細鍙栨渶鐭棆杞矾寰?
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
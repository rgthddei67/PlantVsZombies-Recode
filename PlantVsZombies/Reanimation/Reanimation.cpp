#include "Reanimation.h"
#include "../ResourceManager.h"
#include <iostream>
#include "../FileManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

            // 初始化前一帧的数据（原版中的aPrevXXX变量）
            float prevX = 0.0f;
            float prevY = 0.0f;
            float prevKx = 0.0f;
            float prevKy = 0.0f;
            float prevSx = 1.0f;
            float prevSy = 1.0f;
            float prevA = 1.0f;
            int prevF = 0;
            std::string prevI = "";

            for (pugi::xml_node child : node.children()) {
                std::string childName = child.name();
                if (childName == "name") {
                    track.mTrackName = child.text().as_string();
                }
                else if (childName == "t") {
                    TrackFrameTransform frameTransform;

                    // 初始化所有字段为占位符值
                    frameTransform.x = REANIM_MISSING_FIELD;
                    frameTransform.y = REANIM_MISSING_FIELD;
                    frameTransform.kx = REANIM_MISSING_FIELD;
                    frameTransform.ky = REANIM_MISSING_FIELD;
                    frameTransform.sx = REANIM_MISSING_FIELD;
                    frameTransform.sy = REANIM_MISSING_FIELD;
                    frameTransform.a = REANIM_MISSING_FIELD;
                    frameTransform.f = -1;  // 使用-1作为f字段的占位符
                    frameTransform.i = "";  // 空字符串作为i字段的占位符

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
                            frameTransform.i = prop.text().as_string();
                        }
                    }

                    // 如果当前帧的值是占位符，使用前一帧的值
                    // 否则，更新前一帧的值为当前帧的值
                    if (frameTransform.x == REANIM_MISSING_FIELD)
                        frameTransform.x = prevX;
                    else
                        prevX = frameTransform.x;

                    if (frameTransform.y == REANIM_MISSING_FIELD)
                        frameTransform.y = prevY;
                    else
                        prevY = frameTransform.y;

                    if (frameTransform.kx == REANIM_MISSING_FIELD)
                        frameTransform.kx = prevKx;
                    else
                        prevKx = frameTransform.kx;

                    if (frameTransform.ky == REANIM_MISSING_FIELD)
                        frameTransform.ky = prevKy;
                    else
                        prevKy = frameTransform.ky;

                    if (frameTransform.sx == REANIM_MISSING_FIELD)
                        frameTransform.sx = prevSx;
                    else
                        prevSx = frameTransform.sx;

                    if (frameTransform.sy == REANIM_MISSING_FIELD)
                        frameTransform.sy = prevSy;
                    else
                        prevSy = frameTransform.sy;

                    if (frameTransform.a == REANIM_MISSING_FIELD)
                        frameTransform.a = prevA;
                    else
                        prevA = frameTransform.a;

                    if (frameTransform.f == -1)
                        frameTransform.f = prevF;
                    else
                        prevF = frameTransform.f;

                    if (frameTransform.i.empty())
                        frameTransform.i = prevI;
                    else
                        prevI = frameTransform.i;

                    // 加载图片资源
                    if (!frameTransform.i.empty()) {
                        if (mImagesSet) {
                            mImagesSet->insert(frameTransform.i);
                            if (mResourceManager) {
                                // 检查是否是 REANIM 图片格式
                                if (frameTransform.i.find("IMAGE_REANIM_") == 0) {
                                    // 提取实际文件名：去掉 "IMAGE_REANIM_" 前缀
                                    std::string fileName = frameTransform.i.substr(13);
                                    std::string filePath = "./resources/image/reanim/" + fileName;

                                    SDL_Texture* texture = mResourceManager->LoadTexture(filePath + ".png", frameTransform.i);
                                    if (!texture) {
                                        texture = mResourceManager->LoadTexture(filePath + ".jpg", frameTransform.i);
                                    }

                                    if (!texture) {
                                        std::cout << "警告: 无法加载动画图片: " << frameTransform.i << std::endl;
                                    }
                                }
                            }
                        }
                    }

                    // 添加帧到轨道
                    track.mFrames.push_back(frameTransform);
                }
            }

            // 设置轨道可用性（如果有有效帧）
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

void TransformToMatrix(const TrackFrameTransform& src, glm::mat4& dest,
    float scaleX, float scaleY, float offsetX, float offsetY) {

    dest = glm::mat4(1.0f);

    // 应用位移（包括偏移和全局缩放）
    dest = glm::translate(dest,
        glm::vec3((src.x + offsetX) * scaleX, (src.y + offsetY) * scaleY, 0.0f));

    // 应用旋转（转换为弧度，注意原版可能是负角度）
    dest = glm::rotate(dest, glm::radians(src.kx), glm::vec3(0, 0, 1));

    // 应用缩放（包括轨道缩放和全局缩放）
    dest = glm::scale(dest,
        glm::vec3(src.sx * scaleX, src.sy * scaleY, 1.0f));
}

void GetDeltaTransform(const TrackFrameTransform& tSrc, const TrackFrameTransform& tDst,
    float tDelta, TrackFrameTransform& tOutput, bool useDestFrame) {

    tOutput.a = (tDst.a - tSrc.a) * tDelta + tSrc.a;
    tOutput.x = (tDst.x - tSrc.x) * tDelta + tSrc.x;
    tOutput.y = (tDst.y - tSrc.y) * tDelta + tSrc.y;
    tOutput.sx = (tDst.sx - tSrc.sx) * tDelta + tSrc.sx;
    tOutput.sy = (tDst.sy - tSrc.sy) * tDelta + tSrc.sy;
    tOutput.kx = (tDst.kx - tSrc.kx) * tDelta + tSrc.kx;
    tOutput.ky = (tDst.ky - tSrc.ky) * tDelta + tSrc.ky;

    if (useDestFrame)
        tOutput.f = tDst.f;
    else
        tOutput.f = tSrc.f;

    tOutput.i = tSrc.i;

    // 处理旋转插值时的角度环绕问题
    if (tDst.kx > tSrc.kx + 180.0f)
        tOutput.kx = tSrc.kx;
    if (tDst.kx < tSrc.kx - 180.0f)
        tOutput.kx = tSrc.kx;
    if (tDst.ky > tSrc.ky + 180.0f)
        tOutput.ky = tSrc.ky;
    if (tDst.ky < tSrc.ky - 180.0f)
        tOutput.ky = tSrc.ky;
}
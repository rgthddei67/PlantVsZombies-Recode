#include "Reanimation.h"
#include "../ResourceManager.h"
#include <iostream>
#include "../FileManager.h"

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
            int frameCount = 0;
            TrackFrameTransform currentTransform; // 当前transform状态

            // 初始化默认值
            currentTransform.x = 0.0f;
            currentTransform.y = 0.0f;
            currentTransform.kx = 0.0f;
            currentTransform.ky = 0.0f;
            currentTransform.sx = 1.0f;
            currentTransform.sy = 1.0f;
            currentTransform.a = 1.0f;
            currentTransform.f = 0; // 默认可见
            currentTransform.i = "";

            for (pugi::xml_node child : node.children()) {
                std::string childName = child.name();
                if (childName == "name") {
                    track.mTrackName = child.text().as_string();
                }
                else if (childName == "t") {
                    TrackFrameTransform frameTransform = currentTransform; // 继承当前状态

                    // 解析变换属性 - 只更新有值的属性
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
                            if (!frameTransform.i.empty() && mImagesSet) {
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
#ifdef _DEBUG
                                        if (texture) {
                                            std::cout << "成功加载动画图片: " << frameTransform.i << " -> " << filePath << std::endl;
                                        }
                                        else {
                                            std::cout << "警告: 无法加载动画图片: " << frameTransform.i << std::endl;
                                        }
#endif
                                    }
                                }
                            }
                        }
                    }

                    // 添加帧到轨道
                    track.mFrames.push_back(frameTransform);

                    // 更新当前transform状态，用于下一帧
                    currentTransform = frameTransform;

                    frameCount++;
                }
            }

            // 设置轨道可用性（如果有有效帧）
            track.mAvailable = !track.mFrames.empty();

            mTracks->push_back(track);
        }
    }

    mIsLoaded = true;
    std::cout << "成功加载reanim文件: " << filePath
        << "，轨道数: " << mTracks->size()
        << "，总帧数: " << GetTotalFrames() << std::endl;
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

// Made by AI
void GetDeltaTransform(const TrackFrameTransform& tSrc, const TrackFrameTransform& tDst,
    float tDelta, TrackFrameTransform& tOutput, bool useDestFrame) {

    // 平滑插值函数
    float smoothDelta = tDelta * tDelta * (3.0f - 2.0f * tDelta); // 三次缓动曲线

    // 基本属性插值
    tOutput.a = (tDst.a - tSrc.a) * smoothDelta + tSrc.a;

    // 位置插值
    float rawX = (tDst.x - tSrc.x) * smoothDelta + tSrc.x;
    float rawY = (tDst.y - tSrc.y) * smoothDelta + tSrc.y;

    // 像素对齐 四舍五入到最近的整数，避免子像素渲染导致的模糊
    tOutput.x = std::round(rawX);
    tOutput.y = std::round(rawY);

    // 缩放插值
    const float ANTI_GAP = 0.001f;
    tOutput.sx = std::max(0.001f, (tDst.sx - tSrc.sx) * smoothDelta + tSrc.sx + ANTI_GAP);
    tOutput.sy = std::max(0.001f, (tDst.sy - tSrc.sy) * smoothDelta + tSrc.sy + ANTI_GAP);

    // 角度插值（处理360度环绕）
    float angleDiffX = tDst.kx - tSrc.kx;
    float angleDiffY = tDst.ky - tSrc.ky;

    // 确保角度差在[-180, 180]范围内，避免不必要的旋转
    if (angleDiffX > 180.0f) angleDiffX -= 360.0f;
    if (angleDiffX < -180.0f) angleDiffX += 360.0f;
    if (angleDiffY > 180.0f) angleDiffY -= 360.0f;
    if (angleDiffY < -180.0f) angleDiffY += 360.0f;

    // 使用线性插值，但可以改用球面线性插值以获得更平滑的旋转
    tOutput.kx = tSrc.kx + angleDiffX * smoothDelta;
    tOutput.ky = tSrc.ky + angleDiffY * smoothDelta;

    // 处理图像资源
    // 如果需要使用目标帧的图像资源，则使用目标帧的i；否则使用源帧的i
    // 但这里有一个逻辑：通常我们在过渡期间保持源帧的图像，除非特别指定
    if (useDestFrame) {
        tOutput.f = tDst.f;
        tOutput.i = tDst.i;
    }
    else {
        tOutput.f = tSrc.f;
        tOutput.i = tSrc.i;
    }

    // 确保透明度在有效范围内
    tOutput.a = std::clamp(tOutput.a, 0.0f, 1.0f);

    // 确保缩放值合理，避免为零或负值
    tOutput.sx = std::max(0.001f, tOutput.sx);
    tOutput.sy = std::max(0.001f, tOutput.sy);
}
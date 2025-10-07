#include "ReanimParser.h"
#include "ResourceManager.h"
#include "TodDebug.h"
#include <SDL_image.h>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;
std::string ReanimParser::sCurrentBasePath;
std::unique_ptr<XflParser> ReanimParser::sCurrentXflParser = nullptr;

bool ReanimParser::LoadReanimFile(const std::string& filename, ReanimatorDefinition* definition) {
    sCurrentXflParser = std::make_unique<XflParser>();

    if (!sCurrentXflParser->LoadXflFile(filename)) {
        TOD_TRACE("Failed to load XFL file: " + filename);
        sCurrentXflParser.reset();  // 重置指针
        return false;
    }

    // 保存基础路径
    std::string pathToUse;

    // 检查是否是文件路径（包含.xml或.xfl）
    if (filename.find(".xml") != std::string::npos || filename.find(".xfl") != std::string::npos) {
        // 是文件路径，提取目录部分
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            pathToUse = filename.substr(0, lastSlash + 1); // 包含最后的斜杠
#ifdef _DEBUG
            TOD_TRACE("File path detected, extracted directory: " + pathToUse);
#endif
        }
        else {
            pathToUse = "./"; // 当前目录
        }
    }
    else {
        // 是目录路径，直接使用
        pathToUse = filename;
#ifdef _DEBUG
        TOD_TRACE("Directory path detected: " + pathToUse);
#endif
    }

    // 确保路径以分隔符结尾
    if (!pathToUse.empty() && pathToUse.back() != '/' && pathToUse.back() != '\\') {
        pathToUse += "/";
    }

    sCurrentBasePath = pathToUse;
#ifdef _DEBUG
    TOD_TRACE("Final base path set to: " + sCurrentBasePath);
#endif

    const XflDOMDocument& document = sCurrentXflParser->GetDocument();
    const XflDOMTimeline& timeline = document.timeline;

    // 设置FPS
    definition->mFPS = document.frameRate;
#ifdef _DEBUG
    TOD_TRACE("Parsed XFL FPS: " + std::to_string(definition->mFPS));
    TOD_TRACE("Document size: " + std::to_string(document.width) + "x" + std::to_string(document.height));
    TOD_TRACE("Timeline layers: " + std::to_string(timeline.layers.size()));
#endif
    // 为每个图层创建轨道
    int trackCount = 0;
    for (size_t layerIndex = 0; layerIndex < timeline.layers.size(); layerIndex++) {
        const XflDOMLayer& layer = timeline.layers[layerIndex];

        if (!layer.visible) {
#ifdef _DEBUG
            TOD_TRACE("Skipping invisible layer: " + layer.name);
#endif
            continue;
        }

        CreateTrackFromXflLayer(layer, definition, static_cast<int>(layerIndex), *sCurrentXflParser);
        trackCount++;
    }
#ifdef _DEBUG
    TOD_TRACE("Successfully loaded XFL: " + filename + " with " +
        std::to_string(trackCount) + " tracks (total tracks: " +
        std::to_string(definition->mTracks.size()) + ")");
#endif
    return true;
}

void ReanimParser::Cleanup() 
{

}

void ReanimParser::CreateTrackFromXflLayer(const XflDOMLayer& layer, ReanimatorDefinition* definition, int trackIndex, const XflParser& parser) {
    ReanimatorTrack track;
    track.mName = layer.name;

    // 计算总帧数
    int totalFrames = 0;
    for (const auto& frame : layer.frames) {
        int endFrame = frame.index + frame.duration;
        if (endFrame > totalFrames) {
            totalFrames = endFrame;
        }
    }

    // 如果没有帧，创建默认帧
    if (totalFrames == 0) {
        totalFrames = 1;
        TOD_TRACE("No frames found in layer " + layer.name + ", creating default frame");
    }

    // 初始化所有帧的变换
    track.mTransforms.resize(totalFrames);

    // 解析每个关键帧
    for (const auto& frame : layer.frames) {
        for (int i = 0; i < frame.duration; i++) {
            int currentFrame = frame.index + i;
            if (currentFrame < static_cast<int>(track.mTransforms.size())) {
                ParseXflFrame(frame, track.mTransforms[currentFrame], currentFrame, parser);
            }
        }
    }

    // XFL中图层是从上到下的顺序，但渲染时应该从下到上
    // 所以我们将图层逆序插入，确保正确的渲染顺序
    definition->mTracks.insert(definition->mTracks.begin(), track);
#ifdef _DEBUG
    TOD_TRACE("Created track from layer: " + layer.name + " at position " +
        std::to_string(definition->mTracks.size() - 1) + " with " +
        std::to_string(track.mTransforms.size()) + " frames");
#endif
}

bool ReanimParser::ParseXflFrame(const XflDOMFrame& frame, ReanimatorTransform& transform, int frameIndex, const XflParser& parser) {
    // 初始化默认值
    transform.mTransX = 0.0f;
    transform.mTransY = 0.0f;
    transform.mSkewX = 0.0f;
    transform.mSkewY = 0.0f;
    transform.mScaleX = 1.0f;
    transform.mScaleY = 1.0f;
    transform.mAlpha = 1.0f;
    transform.mFrame = -1.0f;
    transform.mImage = nullptr;
    transform.mImageName = "";

    // 处理帧中的元素
    for (const auto& elementName : frame.elements) {
        ProcessXflSymbol(elementName, parser, transform);

        // 对于这个帧，尝试获取特定帧的变换
        XflElement frameSpecificTransform;
        if (parser.GetElementTransform(elementName, frameIndex, frameSpecificTransform)) {
            transform.mTransX = frameSpecificTransform.x;
            transform.mTransY = frameSpecificTransform.y;
            transform.mScaleX = frameSpecificTransform.scaleX;
            transform.mScaleY = frameSpecificTransform.scaleY;
            transform.mSkewX = frameSpecificTransform.rotation;
            transform.mAlpha = frameSpecificTransform.alpha;
#ifdef _DEBUG
            TOD_TRACE("Frame " + std::to_string(frameIndex) + " transform for " + elementName +
                ": Pos(" + std::to_string(transform.mTransX) + ", " +
                std::to_string(transform.mTransY) + ")");
#endif
        }
    }
#ifdef _DEBUG
    TOD_TRACE("Parsed XFL frame " + std::to_string(frameIndex) +
        " with " + std::to_string(frame.elements.size()) + " elements" +
        " Final Position: (" + std::to_string(transform.mTransX) + ", " +
        std::to_string(transform.mTransY) + ")");
#endif 
    return true;
}

void ReanimParser::ProcessXflSymbol(const std::string& symbolName, const XflParser& parser, ReanimatorTransform& transform) {
    // 检查是否是位图
    std::string bitmapPath = parser.GetBitmapPath(symbolName);

    transform.mImageName = symbolName;
    transform.mFrame = 0;

    // 尝试从XFL获取变换信息
    XflElement elementTransform;
    if (parser.GetElementTransform(symbolName, 0, elementTransform)) {
        // 使用XFL中的实际变换数据
        transform.mTransX = elementTransform.x;
        transform.mTransY = elementTransform.y;
        transform.mScaleX = elementTransform.scaleX;
        transform.mScaleY = elementTransform.scaleY;
        transform.mSkewX = elementTransform.rotation;
        transform.mAlpha = elementTransform.alpha;
#ifdef _DEBUG
        TOD_TRACE(" Applied XFL transform for " + symbolName +
            ": Pos(" + std::to_string(transform.mTransX) + ", " +
            std::to_string(transform.mTransY) + ")" +
            " Scale(" + std::to_string(transform.mScaleX) + ", " +
            std::to_string(transform.mScaleY) + ")" +
            " Rotation: " + std::to_string(transform.mSkewX) + "°" +
            " Alpha: " + std::to_string(transform.mAlpha));
#endif
    }
    else {
        // 如果没有找到变换数据，使用默认值
        transform.mTransX = 0.0f;
        transform.mTransY = 0.0f;
        transform.mScaleX = 1.0f;
        transform.mScaleY = 1.0f;
        transform.mSkewX = 0.0f;
        transform.mAlpha = 1.0f;
#ifdef _DEBUG
        TOD_TRACE(" Using default transform for " + symbolName);
#endif
    }
#ifdef _DEBUG
    if (!bitmapPath.empty()) {
        TOD_TRACE(" Found bitmap: " + symbolName + " at " + bitmapPath);
    }
    else {
        TOD_TRACE(" Bitmap not found: " + symbolName);
    }
#endif 
}

SDL_Texture* ReanimParser::LoadXflBitmap(const std::string& bitmapPath, SDL_Renderer* renderer) {
    if (!renderer || bitmapPath.empty()) {
        TOD_TRACE("Invalid parameters for loading XFL bitmap");
        return nullptr;
    }

    // 直接加载位图文件
    SDL_Surface* surface = IMG_Load(bitmapPath.c_str());
    if (!surface) {
        TOD_TRACE("Failed to load XFL bitmap: " + bitmapPath + " - " + IMG_GetError());
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        TOD_TRACE("Failed to create texture from XFL bitmap: " + std::string(SDL_GetError()));
        return nullptr;
    }
#ifdef _DEBUG
    TOD_TRACE("Successfully loaded XFL bitmap: " + bitmapPath);
#endif
    return texture;
}

SDL_Texture* ReanimParser::LoadReanimImage(const std::string& imageName, SDL_Renderer* renderer) {
    if (!renderer) {
        TOD_TRACE("No renderer provided for loading image: " + imageName);
        return nullptr;
    }

    if (imageName.empty()) {
        TOD_TRACE("Empty image name provided");
        return nullptr;
    }

    // 提取文件名
    std::string fileName = ExtractFileNameFromImageName(imageName);
    if (fileName.empty()) {
        TOD_TRACE("Failed to extract file name from: " + imageName);
        return nullptr;
    }
#ifdef _DEBUG
    TOD_TRACE("Loading image - Original: " + imageName + ", Extracted: " + fileName + ", BasePath: " + sCurrentBasePath);
#endif
    // 使用保存的基础路径查找文件
    std::string filePath = FindImageFile(fileName, sCurrentBasePath);

    if (filePath.empty()) {
        TOD_TRACE("Could not find image file for: " + fileName + " (derived from: " + imageName + ")");
        return nullptr;
    }

    // 加载图像
    SDL_Surface* surface = IMG_Load(filePath.c_str());
    if (!surface) {
        TOD_TRACE("Failed to load image: " + filePath + " - " + IMG_GetError());
        return nullptr;
    }

    // 创建纹理
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        TOD_TRACE("Failed to create texture from surface: " + std::string(SDL_GetError()));
        return nullptr;
    }
#ifdef _DEBUG
    TOD_TRACE("Successfully loaded image: " + imageName + " -> " + filePath);
#endif
    return texture;
}

std::string ReanimParser::ExtractFileNameFromImageName(const std::string& imageName) {
    // 处理不同的命名格式

    // 格式1: "IMAGE_REANIM_Sun1" -> "Sun1"
    std::string prefix = "IMAGE_REANIM_";
    if (imageName.find(prefix) == 0) {
        return imageName.substr(prefix.length());
    }

    // 格式2: 直接就是文件名 "Sun1"
    // 检查是否包含路径分隔符
    if (imageName.find('/') == std::string::npos && imageName.find('\\') == std::string::npos) {
        return imageName;
    }

    // 格式3: 包含路径，提取文件名
    size_t lastSlash = imageName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        std::string fileName = imageName.substr(lastSlash + 1);

        // 移除扩展名
        size_t dotPos = fileName.find_last_of('.');
        if (dotPos != std::string::npos) {
            return fileName.substr(0, dotPos);
        }
        return fileName;
    }

    return imageName;
}

std::string ReanimParser::FindImageFile(const std::string& baseName, const std::string& basePath) {
    const std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };

    // 构建搜索路径 - 完全基于传入的basePath
    std::vector<std::string> searchPaths;

    if (!basePath.empty()) {
        // 使用提供的基础路径（与GetBitmapPath完全一致）
        searchPaths = {
            basePath + "LIBRARY/",
            basePath + "library/",
            basePath + "Library/",
            basePath  // 直接在当前目录搜索
        };
    }

    // 添加通用的回退路径（不包含具体动画名称）
    searchPaths.insert(searchPaths.end(), {
        "./resources/reanim/LIBRARY/",
        "./resources/reanim/library/",
        "./LIBRARY/",
        "./library/",
        "./resources/reanim/",
        "./"
        });

    std::string baseNameLower = baseName;
    std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), ::tolower);
#ifdef _DEBUG
    TOD_TRACE("Searching for image: " + baseName + " with base path: " + basePath);
#endif
    for (const auto& path : searchPaths) {
        for (const auto& ext : extensions) {
            // 尝试原始文件名
            std::string fullPath = path + baseName + ext;
            std::ifstream file(fullPath);
            if (file.good()) {
                file.close();
#ifdef _DEBUG
                TOD_TRACE("Found image file: " + fullPath);
#endif
                return fullPath;
            }

            // 尝试小写版本
            std::string fullPathLower = path + baseNameLower + ext;
            file.open(fullPathLower);
            if (file.good()) {
                file.close();
#ifdef _DEBUG
                TOD_TRACE("Found image file (lowercase): " + fullPathLower);
#endif
                return fullPathLower;
            }
        }
    }

    TOD_TRACE("Image file not found for: " + baseName + " in base path: " + basePath);
    return "";
}
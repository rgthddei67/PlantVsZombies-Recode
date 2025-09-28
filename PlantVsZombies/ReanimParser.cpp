#include "ReanimParser.h"
#include "ResourceManager.h"
#include "TodDebug.h"
#include <SDL_image.h>

// 从图片名称提取文件名
std::string ReanimParser::ExtractFileNameFromImageName(const std::string& imageName) {
    // 移除 "IMAGE_REANIM_" 前缀
    std::string prefix = "IMAGE_REANIM_";
    if (imageName.find(prefix) == 0) {
        return imageName.substr(prefix.length());
    }
    // 如果没有前缀，直接返回原名称
    return imageName;
}

// 在目录中查找图片文件（不区分大小写）
std::string ReanimParser::FindImageFile(const std::string& baseName) {
    // 支持的图片格式
    const std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };

    // 搜索目录
    const std::vector<std::string> searchPaths = {
        "./resources/reanim/images",
    };

    // 将基础名称转换为小写用于比较
    std::string baseNameLower = baseName;
    std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), ::tolower);

    for (const auto& path : searchPaths) 
    {
        for (const auto& ext : extensions) {
            std::string fullPath = path + "/" + baseName + ext;

            // 检查文件是否存在
            std::ifstream file(fullPath);
            if (file.good()) {
                file.close();
                TOD_TRACE("Found image file: " + fullPath);
                return fullPath;
            }

            // 尝试小写版本
            std::string fullPathLower = path + baseNameLower + ext;
            file.open(fullPathLower);
            if (file.good()) {
                file.close();
                TOD_TRACE("Found image file (lowercase): " + fullPathLower);
                return fullPathLower;
            }
        }
    }

    TOD_TRACE("Image file not found for: " + baseName);
    return "";
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

    // 从图片名称提取文件名
    std::string fileName = ExtractFileNameFromImageName(imageName);
    if (fileName.empty()) {
        TOD_TRACE("Failed to extract file name from: " + imageName);
        return nullptr;
    }

    // 查找图片文件
    std::string filePath = FindImageFile(fileName);
    if (filePath.empty()) {
        TOD_TRACE("Could not find image file for: " + fileName + " (derived from: " + imageName + ")");
        return nullptr;
    }

    // 加载图片
    SDL_Surface* surface = IMG_Load(filePath.c_str());
    if (!surface) {
        TOD_TRACE("Failed to load image: " + filePath + " - " + IMG_GetError());
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        TOD_TRACE("Failed to create texture from surface: " + std::string(SDL_GetError()));
        return nullptr;
    }

    TOD_TRACE("Successfully loaded image: " + imageName + " -> " + filePath);
    return texture;
}

bool ReanimParser::LoadReanimFile(const std::string& filename, ReanimatorDefinition* definition) {
    SimpleXmlNode root;
    if (!SimpleXmlParser::ParseFile(filename, root)) {
        TOD_TRACE("Failed to load reanim file: " + filename);
        return false;
    }

    // 解析FPS
    for (auto& child : root.children) {
        if (child.name == "fps") {
            definition->mFPS = ParseFloatValue(child.text, 12.0f);
            TOD_TRACE("Parsed FPS: " + std::to_string(definition->mFPS));
            break;
        }
    }

    // 解析轨道
    int trackCount = 0;
    for (auto& trackNode : root.children) {
        if (trackNode.name != "track") continue;

        ReanimatorTrack track;
        trackCount++;

        // 获取轨道名称
        for (auto& nameChild : trackNode.children) {
            if (nameChild.name == "name") {
                track.mName = nameChild.text;
                TOD_TRACE("Processing track: " + track.mName);
                break;
            }
        }

        // 解析关键帧
        int transformCount = 0;
        for (auto& tNode : trackNode.children) {
            if (tNode.name != "t") continue;

            ReanimatorTransform transform;
            if (ParseTransform(&tNode, transform)) {
                track.mTransforms.push_back(transform);
                transformCount++;

                // 调试信息
                if (!transform.mImageName.empty()) {
                    TOD_TRACE("Transform " + std::to_string(transformCount) +
                        " has image: " + transform.mImageName);
                }
            }
        }

        TOD_TRACE("Parsed " + std::to_string(transformCount) + " transforms for track: " + track.mName);

        if (!track.mTransforms.empty()) {
            definition->mTracks.push_back(track);
        }
    }

    TOD_TRACE("Successfully loaded reanim: " + filename + " with " +
        std::to_string(definition->mTracks.size()) + " tracks and " +
        std::to_string(trackCount) + " track nodes found");
    return true;
}

bool ReanimParser::ParseTransform(const SimpleXmlNode* tNode, ReanimatorTransform& transform) {
    // 初始化默认值
    transform.mTransX = 0.0f;
    transform.mTransY = 0.0f;
    transform.mSkewX = 0.0f;
    transform.mSkewY = 0.0f;
    transform.mScaleX = 1.0f;
    transform.mScaleY = 1.0f;
    transform.mAlpha = 1.0f;
    transform.mFrame = -1.0f;   // -1.0f表示没有图像
    transform.mImage = nullptr;
    transform.mImageName = "";

    bool hasImage = false;
    // 解析子元素
    for (auto& child : tNode->children) {
        if (child.name == "x") {
            transform.mTransX = ParseFloatValue(child.text, 0);
            //TOD_TRACE("  x = " + std::to_string(transform.mTransX));
        }
        else if (child.name == "y") {
            transform.mTransY = ParseFloatValue(child.text, 0);
            //TOD_TRACE("  y = " + std::to_string(transform.mTransY));
        }
        else if (child.name == "kx") {
            transform.mSkewX = ParseFloatValue(child.text, 0);
        }
        else if (child.name == "ky") {
            transform.mSkewY = ParseFloatValue(child.text, 0);
        }
        else if (child.name == "sx") {
            transform.mScaleX = ParseFloatValue(child.text, 1.0f);
        }
        else if (child.name == "sy") {
            transform.mScaleY = ParseFloatValue(child.text, 1.0f);
        }
        else if (child.name == "i") {
            if (!child.text.empty()) {
                transform.mImageName = child.text;
                transform.mFrame = 0;  // 有图像时设置为0
                hasImage = true;
                TOD_TRACE("Found image: " + transform.mImageName);
            }
        }
        else if (child.name == "a") {
            transform.mAlpha = ParseFloatValue(child.text, 1.0f);
        }
    }

    return true;
}

float ReanimParser::ParseFloatValue(const std::string& value, float defaultValue) {
    if (value.empty()) return defaultValue;
    try {
        return std::stof(value);
    }
    catch (...) {
        return defaultValue;
    }
}

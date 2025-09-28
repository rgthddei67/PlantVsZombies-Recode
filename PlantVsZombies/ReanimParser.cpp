#include "ReanimParser.h"
#include "ResourceManager.h"
#include "TodDebug.h"
#include <SDL_image.h>

// ��ͼƬ������ȡ�ļ���
std::string ReanimParser::ExtractFileNameFromImageName(const std::string& imageName) {
    // �Ƴ� "IMAGE_REANIM_" ǰ׺
    std::string prefix = "IMAGE_REANIM_";
    if (imageName.find(prefix) == 0) {
        return imageName.substr(prefix.length());
    }
    // ���û��ǰ׺��ֱ�ӷ���ԭ����
    return imageName;
}

// ��Ŀ¼�в���ͼƬ�ļ��������ִ�Сд��
std::string ReanimParser::FindImageFile(const std::string& baseName) {
    // ֧�ֵ�ͼƬ��ʽ
    const std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };

    // ����Ŀ¼
    const std::vector<std::string> searchPaths = {
        "./resources/reanim/images",
    };

    // ����������ת��ΪСд���ڱȽ�
    std::string baseNameLower = baseName;
    std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), ::tolower);

    for (const auto& path : searchPaths) 
    {
        for (const auto& ext : extensions) {
            std::string fullPath = path + "/" + baseName + ext;

            // ����ļ��Ƿ����
            std::ifstream file(fullPath);
            if (file.good()) {
                file.close();
                TOD_TRACE("Found image file: " + fullPath);
                return fullPath;
            }

            // ����Сд�汾
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

    // ��ͼƬ������ȡ�ļ���
    std::string fileName = ExtractFileNameFromImageName(imageName);
    if (fileName.empty()) {
        TOD_TRACE("Failed to extract file name from: " + imageName);
        return nullptr;
    }

    // ����ͼƬ�ļ�
    std::string filePath = FindImageFile(fileName);
    if (filePath.empty()) {
        TOD_TRACE("Could not find image file for: " + fileName + " (derived from: " + imageName + ")");
        return nullptr;
    }

    // ����ͼƬ
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

    // ����FPS
    for (auto& child : root.children) {
        if (child.name == "fps") {
            definition->mFPS = ParseFloatValue(child.text, 12.0f);
            TOD_TRACE("Parsed FPS: " + std::to_string(definition->mFPS));
            break;
        }
    }

    // �������
    int trackCount = 0;
    for (auto& trackNode : root.children) {
        if (trackNode.name != "track") continue;

        ReanimatorTrack track;
        trackCount++;

        // ��ȡ�������
        for (auto& nameChild : trackNode.children) {
            if (nameChild.name == "name") {
                track.mName = nameChild.text;
                TOD_TRACE("Processing track: " + track.mName);
                break;
            }
        }

        // �����ؼ�֡
        int transformCount = 0;
        for (auto& tNode : trackNode.children) {
            if (tNode.name != "t") continue;

            ReanimatorTransform transform;
            if (ParseTransform(&tNode, transform)) {
                track.mTransforms.push_back(transform);
                transformCount++;

                // ������Ϣ
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
    // ��ʼ��Ĭ��ֵ
    transform.mTransX = 0.0f;
    transform.mTransY = 0.0f;
    transform.mSkewX = 0.0f;
    transform.mSkewY = 0.0f;
    transform.mScaleX = 1.0f;
    transform.mScaleY = 1.0f;
    transform.mAlpha = 1.0f;
    transform.mFrame = -1.0f;   // -1.0f��ʾû��ͼ��
    transform.mImage = nullptr;
    transform.mImageName = "";

    bool hasImage = false;
    // ������Ԫ��
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
                transform.mFrame = 0;  // ��ͼ��ʱ����Ϊ0
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

#pragma once
#ifndef _REANIM_PARSER_H
#define _REANIM_PARSER_H

#include "Reanimator.h"
#include "TodCommon.h"
#include "SimpleXmlParser.h" 
#include <string>
#include <vector>
#include <map>

class ReanimParser {
public:
    static bool LoadReanimFile(const std::string& filename, ReanimatorDefinition* definition);
    static bool ParseTransform(const SimpleXmlNode* tNode, ReanimatorTransform& transform);
    static SDL_Texture* LoadReanimImage(const std::string& imageName, SDL_Renderer* renderer);

private:
    static float ParseFloatValue(const std::string& value, float defaultValue);

    // 从图片名称提取文件名
    static std::string ExtractFileNameFromImageName(const std::string& imageName);
    // 在目录中查找图片文件
    static std::string FindImageFile(const std::string& baseName);
};

#endif
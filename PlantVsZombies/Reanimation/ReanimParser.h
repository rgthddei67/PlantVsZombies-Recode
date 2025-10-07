#pragma once
#ifndef _REANIM_PARSER_H
#define _REANIM_PARSER_H
#include "../AllCppInclude.h"
#include "Reanimator.h"
#include "TodCommon.h"
#include "XflParser.h" 
#include <string>
#include <vector>
#include <map>

class ReanimatorDefinition;
struct ReanimatorTransform;

class ReanimParser {
public:
    static bool LoadReanimFile(const std::string& filename, ReanimatorDefinition* definition);
    static void Cleanup();
    static bool ParseXflFrame(const XflDOMFrame& frame, ReanimatorTransform& transform, int frameIndex, const XflParser& parser);
    static SDL_Texture* LoadReanimImage(const std::string& imageName, SDL_Renderer* renderer);
    static SDL_Texture* LoadXflBitmap(const std::string& bitmapPath, SDL_Renderer* renderer);

private:
    static std::string sCurrentBasePath;
    static std::unique_ptr<XflParser> sCurrentXflParser;

    static void CreateTrackFromXflLayer(const XflDOMLayer& layer, ReanimatorDefinition* definition, int trackIndex, const XflParser& parser);
    static void ProcessXflSymbol(const std::string& symbolName, const XflParser& parser, ReanimatorTransform& transform);

    // Õº∆¨≤È’“π¶ƒ‹
    static std::string ExtractFileNameFromImageName(const std::string& imageName);
    static std::string FindImageFile(const std::string& baseName, const std::string& basePath = "");
};

#endif
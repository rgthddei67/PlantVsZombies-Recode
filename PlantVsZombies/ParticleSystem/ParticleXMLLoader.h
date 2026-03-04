#pragma once
#ifndef __PARTICLE_XML_LOADER_H__
#define __PARTICLE_XML_LOADER_H__

#include "ParticleXMLConfig.h"
#include "pugixml.hpp"
#include <string>
#include <unordered_map>
#include <memory>

class ParticleXMLLoader {
private:
    std::unordered_map<std::string, ParticleEffectConfig> effectConfigs;

public:
    ParticleXMLLoader() = default;
    ~ParticleXMLLoader() = default;

    // 从目录加载所有XML文件
    bool LoadFromDirectory(const std::string& directory);

    // 从单个文件加载
    bool LoadFromFile(const std::string& filePath);

    // 获取特效配置
    const ParticleEffectConfig* GetEffectConfig(const std::string& name) const;

    // 获取所有配置名称
    std::vector<std::string> GetAllEffectNames() const;

private:
    // 解析辅助函数
    InterpolationTrack ParseInterpolationTrack(const std::string& text);
    ValueRange ParseValueRange(const std::string& text);
    EmitterConfig ParseEmitter(const pugi::xml_node& emitterNode);
    ParticleField ParseField(const pugi::xml_node& fieldNode);
    ParticleFieldType ParseFieldType(const std::string& typeStr);
    EmitterType ParseEmitterType(const std::string& typeStr);

    // 解析多个纹理键（逗号分隔）
    std::vector<std::string> ParseImageKeys(const std::string& text);
};

#endif
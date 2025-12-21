#pragma once
#ifndef _PLANT_DATA_MANAGER_H
#define _PLANT_DATA_MANAGER_H

#include "PlantType.h"
#include "../../Reanimation/AnimationTypes.h"
#include "../Definit.h"
#include <string>
#include <unordered_map>
#include <vector>

struct PlantInfo {
    PlantType type;                     // 植物类型
    std::string enumName;               // 枚举名
    std::string textureKey;             // 纹理键
    AnimationType animType;             // 动画类型
    std::string animName;               // 动画名称
    Vector offset;                      // 偏移量

    PlantInfo() : type(PlantType::PLANT_NONE),
        animType(AnimationType::ANIM_NONE),
        offset(0, 0) {
    }

    PlantInfo(PlantType t, const std::string& enumN,
        const std::string& tex, AnimationType animT,
        const std::string& animN, const Vector& off)
        : type(t), enumName(enumN), textureKey(tex),
        animType(animT), animName(animN), offset(off) {
    }
};

class PlantDataManager {
public:
    static PlantDataManager& GetInstance() {
        static PlantDataManager instance;
        return instance;
    }

    PlantDataManager(const PlantDataManager&) = delete;
    PlantDataManager& operator=(const PlantDataManager&) = delete;

    void Initialize();

    std::string GetPlantTextureKey(PlantType plantType) const;
    AnimationType GetPlantAnimationType(PlantType plantType) const;
    std::string GetAnimationName(AnimationType animType) const;
    Vector GetPlantOffset(PlantType plantType) const;

    void SetPlantOffset(PlantType plantType, const Vector& offset);

    std::string PlantTypeToEnumName(PlantType type) const;
    std::string PlantTypeToTextureKey(PlantType type) const;
    std::string PlantTypeToAnimName(PlantType type) const;
    PlantType StringToPlantType(const std::string& str) const;

    std::vector<PlantType> GetAllPlantTypes() const;
    bool HasPlant(PlantType type) const;

    void DebugPrintAll() const;

private:
    // 私有构造函数
    PlantDataManager();

    // 核心数据映射
    std::unordered_map<PlantType, PlantInfo> mPlantInfo;
    std::unordered_map<AnimationType, std::string> mAnimToString;
    std::unordered_map<std::string, PlantType> mEnumNameToType;
    std::unordered_map<std::string, PlantType> mTextureKeyToType;
    std::unordered_map<std::string, PlantType> mAnimNameToType;

    // 硬编码初始化所有植物数据
    void InitializeHardcodedData();

    // 注册植物
    void RegisterPlant(PlantType type,
        const std::string& enumName,
        const std::string& textureKey,
        AnimationType animType,
        const std::string& animName,
        const Vector& offset);
};

#endif
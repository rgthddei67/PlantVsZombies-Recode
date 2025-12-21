#include "PlantDataManager.h"
#include <iostream>
#include <algorithm>
#include <vector>

PlantDataManager::PlantDataManager() {

}

void PlantDataManager::Initialize() {
    // 清空现有数据
    mPlantInfo.clear();
    mAnimToString.clear();
    mEnumNameToType.clear();
    mTextureKeyToType.clear();
    mAnimNameToType.clear();

    InitializeHardcodedData();

    std::cout << "[PlantDataManager] 初始化完成，共注册 "
        << mPlantInfo.size() << " 种植物" << std::endl;
}

void PlantDataManager::InitializeHardcodedData() {
    // TODO: 新增植物、动画时，只需要在这里改和动画Type

    // 向日葵
    RegisterPlant(
        PlantType::PLANT_SUNFLOWER,
        "PLANT_SUNFLOWER",
        "IMAGE_SunFlower",
        AnimationType::ANIM_SUNFLOWER,
        "SunFlower",
        Vector(-40, -45)  // 偏移量
    );

    // 豌豆射手
    RegisterPlant(
        PlantType::PLANT_PEASHOOTER,
        "PLANT_PEASHOOTER",
        "IMAGE_PeaShooter",
        AnimationType::ANIM_PEASHOOTER,
        "PeaShooter",
        Vector(-40, -45)
    );

    // 樱桃炸弹
    RegisterPlant(
        PlantType::PLANT_CHERRYBOMB,
        "PLANT_CHERRYBOMB",
        "IMAGE_CherryBomb",
        AnimationType::ANIM_CHERRYBOMB,
        "CherryBomb",
        Vector(-40, -45)
    );

    // ==================== 初始化非植物动画映射 ====================
    mAnimToString[AnimationType::ANIM_SUN] = "Sun";
    mAnimToString[AnimationType::ANIM_NONE] = "Unknown";
}

void PlantDataManager::RegisterPlant(PlantType type,
    const std::string& enumName,
    const std::string& textureKey,
    AnimationType animType,
    const std::string& animName,
    const Vector& offset) {
    PlantInfo info(type, enumName, textureKey, animType, animName, offset);
    mPlantInfo[type] = info;

    // 更新反向映射
    mEnumNameToType[enumName] = type;
    mTextureKeyToType[textureKey] = type;
    mAnimNameToType[animName] = type;

    // 更新动画到字符串的映射
    mAnimToString[animType] = animName;

#ifdef _DEBUG
    std::cout << "[PlantDataManager] 注册植物: " << animName
        << " (偏移: " << offset.x << ", " << offset.y << ")" << std::endl;
#endif
}

std::string PlantDataManager::GetPlantTextureKey(PlantType plantType) const {
    auto it = mPlantInfo.find(plantType);
    if (it != mPlantInfo.end()) {
        return it->second.textureKey;
    }
    return "IMAGE_PLANT_DEFAULT";
}

AnimationType PlantDataManager::GetPlantAnimationType(PlantType plantType) const {
    auto it = mPlantInfo.find(plantType);
    if (it != mPlantInfo.end()) {
        return it->second.animType;
    }
    return AnimationType::ANIM_NONE;
}

std::string PlantDataManager::GetAnimationName(AnimationType animType) const {
    // 首先在植物动画中查找
    for (const auto& pair : mPlantInfo) {
        if (pair.second.animType == animType) {
            return pair.second.animName;
        }
    }

    // 然后在非植物动画中查找
    auto it = mAnimToString.find(animType);
    if (it != mAnimToString.end()) {
        return it->second;
    }

    return "Unknown";
}

Vector PlantDataManager::GetPlantOffset(PlantType plantType) const {
    auto it = mPlantInfo.find(plantType);
    if (it != mPlantInfo.end()) {
        return it->second.offset;
    }
    return Vector(0, 0);
}

void PlantDataManager::SetPlantOffset(PlantType plantType, const Vector& offset) {
    auto it = mPlantInfo.find(plantType);
    if (it != mPlantInfo.end()) {
        it->second.offset = offset;
#ifdef _DEBUG
        std::cout << "[PlantDataManager] 设置植物偏移: " << it->second.animName
            << " -> (" << offset.x << ", " << offset.y << ")" << std::endl;
#endif
    }
}

std::string PlantDataManager::PlantTypeToEnumName(PlantType type) const {
    auto it = mPlantInfo.find(type);
    if (it != mPlantInfo.end()) {
        return it->second.enumName;
    }
    return "PLANT_NONE";
}

std::string PlantDataManager::PlantTypeToTextureKey(PlantType type) const {
    return GetPlantTextureKey(type);
}

std::string PlantDataManager::PlantTypeToAnimName(PlantType type) const {
    auto it = mPlantInfo.find(type);
    if (it != mPlantInfo.end()) {
        return it->second.animName;
    }
    return "Unknown";
}

PlantType PlantDataManager::StringToPlantType(const std::string& str) const {
    // 尝试枚举名
    auto enumIt = mEnumNameToType.find(str);
    if (enumIt != mEnumNameToType.end()) {
        return enumIt->second;
    }

    // 尝试纹理键
    auto texIt = mTextureKeyToType.find(str);
    if (texIt != mTextureKeyToType.end()) {
        return texIt->second;
    }

    // 尝试动画名
    auto animIt = mAnimNameToType.find(str);
    if (animIt != mAnimNameToType.end()) {
        return animIt->second;
    }

    return PlantType::PLANT_NONE;
}

std::vector<PlantType> PlantDataManager::GetAllPlantTypes() const {
    std::vector<PlantType> types;
    for (const auto& pair : mPlantInfo) {
        types.push_back(pair.first);
    }
    return types;
}

bool PlantDataManager::HasPlant(PlantType type) const {
    return mPlantInfo.find(type) != mPlantInfo.end();
}

void PlantDataManager::DebugPrintAll() const {
    std::cout << "========== PlantDataManager 硬编码数据 ==========" << std::endl;
    std::cout << "植物总数: " << mPlantInfo.size() << std::endl;

    for (const auto& pair : mPlantInfo) {
        const PlantInfo& info = pair.second;
        std::cout << "植物: " << info.animName << " (" << info.enumName << ")" << std::endl;
        std::cout << "  纹理键: " << info.textureKey << std::endl;
        std::cout << "  动画类型: " << static_cast<int>(info.animType) << std::endl;
        std::cout << "  偏移量: (" << info.offset.x << ", " << info.offset.y << ")" << std::endl;
        std::cout << std::endl;
    }

    std::cout << "=============================================" << std::endl;
}
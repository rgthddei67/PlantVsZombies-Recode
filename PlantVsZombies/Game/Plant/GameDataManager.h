#pragma once
#ifndef _PLANT_DATA_MANAGER_H
#define _PLANT_DATA_MANAGER_H

#include "PlantType.h"
#include "../Zombie/ZombieType.h"     
#include "../../Reanimation/AnimationTypes.h"
#include "../Definit.h"
#include <string>
#include <unordered_map>
#include <vector>

// 植物信息
struct PlantInfo {
    PlantType type;             // 植物类型
	int SunCost = 0;                 // 阳光
	float Cooldown = 7.5f;                // 冷却时间（单位：秒）
    std::string enumName;       // 枚举名（字符串形式）
    std::string textureKey;      // 纹理资源键（用于静态图片）
    AnimationType animType;      // 动画类型
    std::string animName;        // 动画资源名称（如 Reanim 名称）
    Vector offset;               // 绘制偏移量

	PlantInfo() : type(PlantType::NUM_PLANT_TYPES),
        animType(AnimationType::ANIM_NONE),
        offset(0, 0) {
    }

    PlantInfo(PlantType t, int sunCost, float cooldown, const std::string& enumN,
        const std::string& tex, AnimationType animT,
        const std::string& animN, const Vector& off)
        : type(t), SunCost(sunCost), Cooldown(cooldown), enumName(enumN), textureKey(tex),
        animType(animT), animName(animN), offset(off) {
    }
};

// 僵尸信息
struct ZombieInfo {
    ZombieType type;            // 僵尸类型
    std::string enumName;       // 枚举名（字符串形式）
    AnimationType animType;      // 动画类型
    std::string animName;        // 动画资源名称
    Vector offset;               // 绘制偏移量
    int weight;                  // 权重
    int appearWave;              // 能出现的波数

    ZombieInfo() : type(ZombieType::NUM_ZOMBIE_TYPES),
        animType(AnimationType::ANIM_NONE),
        offset(0, 0), weight(0), appearWave(0) {
    }

    ZombieInfo(ZombieType t, const std::string& enumN,
        AnimationType animT, const std::string& animN, const Vector& off, int w, int appear)
        : type(t), enumName(enumN), animType(animT), animName(animN),
        offset(off), weight(w), appearWave(appear) {
    }
};

class GameDataManager {
public:
    static GameDataManager& GetInstance() {
        static GameDataManager instance;
        return instance;
    }

    GameDataManager(const GameDataManager&) = delete;
    GameDataManager& operator=(const GameDataManager&) = delete;

    /**
     * @brief 初始化数据管理器，加载硬编码数据
     */
    void Initialize();

    /**
     * @brief 获取植物对应的纹理资源键
     * @param plantType 植物类型
     * @return std::string 纹理键，若未找到返回默认值 "IMAGE_PLANT_DEFAULT"
     */
    std::string GetPlantTextureKey(PlantType plantType) const;

    /**
     * @brief 获取植物对应的动画类型
     * @param plantType 植物类型
     * @return AnimationType 动画类型，若未找到返回 ANIM_NONE
     */
    AnimationType GetPlantAnimationType(PlantType plantType) const;

    /**
     * @brief 通过动画类型获取动画资源名称（适用于植物、僵尸、太阳等）
     * @param animType 动画类型
     * @return std::string 动画资源名，若未找到返回 "Unknown"
     */
    std::string GetAnimationName(AnimationType animType) const;

    /**
     * @brief 获取植物的绘制偏移量
     * @param plantType 植物类型
     * @return Vector 偏移向量，若未找到返回 (0,0)
     */
    Vector GetPlantOffset(PlantType plantType) const;

    /**
     * @brief 设置植物的绘制偏移量（通常用于调试或动态调整）
     * @param plantType 植物类型
     * @param offset 新的偏移量
     */
    void SetPlantOffset(PlantType plantType, const Vector& offset);

    /**
     * @brief 将植物类型转换为对应的枚举名字符串
     * @param type 植物类型
     * @return std::string 枚举名，若未找到返回 "PLANT_NONE"
     */
    std::string PlantTypeToEnumName(PlantType type) const;

    /**
     * @brief 将植物类型转换为纹理键（同 GetPlantTextureKey）
     * @param type 植物类型
     * @return std::string 纹理键
     */
    std::string PlantTypeToTextureKey(PlantType type) const;

    /**
     * @brief 将植物类型转换为动画资源名
     * @param type 植物类型
     * @return std::string 动画资源名，若未找到返回 "Unknown"
     */
    std::string PlantTypeToAnimName(PlantType type) const;

    /**
     * @brief 将字符串解析为植物类型
     * @param str 可以是枚举名、纹理键或动画资源名
     * @return PlantType 对应的植物类型，若未找到返回 NUM_PLANT_TYPES
     */
    PlantType StringToPlantType(const std::string& str) const;

    /**
     * @brief 获取所有已注册的植物类型列表
     * @return std::vector<PlantType> 植物类型数组
     */
    std::vector<PlantType> GetAllPlantTypes() const;

    /**
     * @brief 检查指定植物类型是否已注册
     * @param type 植物类型
     * @return true 已注册，false 未注册
     */
    bool HasPlant(PlantType type) const;

    /**
	 * @brief 获取植物的阳光消耗
	 * @param plantType 植物类型
	 * @return int 阳光消耗，若未找到返回 0
     */
	int GetPlantSunCost(PlantType plantType) const;


    /**
     * @brief 获取植物的冷却时间
     * @param plantType 植物类型
     * @return float 冷却时间（单位：秒），若未找到返回 0.0f
	 */
	float GetPlantCooldown(PlantType plantType) const;

    /**
     * @brief 获取僵尸对应的动画类型
     * @param zombieType 僵尸类型
     * @return AnimationType 动画类型，若未找到返回 ANIM_NONE
     */
    AnimationType GetZombieAnimationType(ZombieType zombieType) const;

    /**
     * @brief 获取僵尸对应的动画资源名
     * @param zombieType 僵尸类型
     * @return std::string 动画资源名，若未找到返回 "Unknown"
     */
    std::string GetZombieAnimName(ZombieType zombieType) const;

    /**
     * @brief 获取僵尸的绘制偏移量
     * @param zombieType 僵尸类型
     * @return Vector 偏移向量，若未找到返回 (0,0)
     */
    Vector GetZombieOffset(ZombieType zombieType) const;

    /**
     * @brief 设置僵尸的绘制偏移量
     * @param zombieType 僵尸类型
     * @param offset 新的偏移量
     */
    void SetZombieOffset(ZombieType zombieType, const Vector& offset);

    /**
     * @brief 获取僵尸的权重
     * @param zombieType 僵尸类型
     */
    int GetZombieWeight(ZombieType zombieType) const;

    /**
     * @brief 获取僵尸的出现波数
     * @param zombieType 僵尸类型
     */
    int GetZombieAppearWave(ZombieType zombieType) const;

    /**
     * @brief 获取所有已注册的僵尸类型列表
     * @return std::vector<ZombieType> 僵尸类型数组
     */
    std::vector<ZombieType> GetAllZombieTypes() const;

    /**
     * @brief 检查指定僵尸类型是否已注册
     * @param type 僵尸类型
     * @return true 已注册，false 未注册
     */
    bool HasZombie(ZombieType type) const;

    /**
     * @brief 打印所有注册数据
     */
    void DebugPrintAll() const;

private:
    GameDataManager();

    /**
     * @brief 硬编码初始化所有植物和僵尸数据
     */
    void InitializeHardcodedData();

    /**
     * @brief 注册一种植物（内部使用）
     * @param type 植物类型
	 * @param sunCost 阳光消耗
	 * @param cooldown 冷却时间（单位：秒）
     * @param enumName 枚举名字符串
     * @param textureKey 纹理键
     * @param animType 动画类型
     * @param animName 动画资源名
     * @param offset 偏移量
     */
    void RegisterPlant(PlantType type,
		int sunCost, float cooldown,
        const std::string& enumName,
        const std::string& textureKey,
        AnimationType animType,
        const std::string& animName,
        const Vector& offset);

    /**
     * @brief 注册一种僵尸（内部使用）
     * @param type 僵尸类型
     * @param enumName 枚举名字符串
     * @param animType 动画类型
     * @param animName 动画资源名
     * @param offset 偏移量
     * @param weight 僵尸权重
     * @param appearWave 能刷新的波数
     */
    void RegisterZombie(ZombieType type,
        const std::string& enumName,
        AnimationType animType,
        const std::string& animName,
        const Vector& offset, int weight, int appearWave);

    // ==================== 数据成员 ====================
    // 植物数据
    std::unordered_map<PlantType, PlantInfo> mPlantInfo;               // 植物类型 -> 植物信息
    std::unordered_map<std::string, PlantType> mEnumNameToType;        // 枚举名 -> 植物类型
    std::unordered_map<std::string, PlantType> mTextureKeyToType;      // 纹理键 -> 植物类型
    std::unordered_map<std::string, PlantType> mAnimNameToType;        // 动画资源名 -> 植物类型

    // 僵尸数据
    std::unordered_map<ZombieType, ZombieInfo> mZombieInfo;             // 僵尸类型 -> 僵尸信息

    // 通用动画类型 -> 资源名映射（可用于植物、僵尸、太阳等）
    std::unordered_map<AnimationType, std::string> mAnimToString;
};

#endif
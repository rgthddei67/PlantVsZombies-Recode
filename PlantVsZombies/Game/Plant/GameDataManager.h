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
#include <memory>

class Board;
class Plant;
class Zombie;

// 工厂函数指针：运行期枚举 → 编译期具体类型。零捕获，可直接由 MakePlant<T>/MakeZombie<T> 赋值。
using PlantFactoryFn  = std::shared_ptr<Plant>(*)(Board*, PlantType, int, int, AnimationType, float, bool);
using ZombieFactoryFn = std::shared_ptr<Zombie>(*)(Board*, ZombieType, float, float, int, AnimationType, float, bool);

// 植物信息
struct PlantInfo {
	PlantType type = PlantType::NUM_PLANT_TYPES;   // 植物类型
	int SunCost = 0;                 // 阳光
	float Cooldown = 7.5f;                // 冷却时间（单位：秒）
	std::string enumName;       // 枚举名（字符串形式）
	std::string textureKey;      // 纹理资源键（用于静态图片）
	AnimationType animType = AnimationType::ANIM_NONE;   // 动画类型
	std::string animName;        // 动画资源名称（如 Reanim 名称）
	Vector offset{ 0, 0 };       // 绘制偏移量
	float scale = 1.0f;          // 创建时的缩放（仅 PotatoMine=0.8，其余=1.0）
	PlantFactoryFn factory = nullptr;  // 具体类的构造工厂

	PlantInfo() = default;
};

// 僵尸信息
struct ZombieInfo {
	ZombieType type = ZombieType::NUM_ZOMBIE_TYPES;   // 僵尸类型
	std::string enumName;       // 枚举名（字符串形式）
	AnimationType animType = AnimationType::ANIM_NONE;   // 动画类型
	std::string animName;        // 动画资源名称
	Vector offset{ 0, 0 };       // 绘制偏移量
	int weight = 0;              // 权重
	int appearWave = 0;          // 能出现的波数
	float scale = 1.0f;          // 创建时的缩放
	int survivalRound = 0;       // 生存模式最早出场轮(1起；0=不进生存，安全默认)
	ZombieFactoryFn factory = nullptr;  // 具体类的构造工厂

	ZombieInfo() = default;
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
	 * @brief 初始化：注册身份数据（硬编码）+ 从 resources/gamedata.json 加载数值
	 * @return false = JSON 缺失/解析失败/缺条目/缺字段（调用方须中止启动）
	 */
	bool Initialize();

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
	 * @brief 获取僵尸的生存模式最早出场轮
	 * @param zombieType 僵尸类型
	 */
	int GetZombieSurvivalRound(ZombieType zombieType) const;

	/**
	 * @brief 将僵尸类型转换为对应的枚举名字符串
	 * @param type 僵尸类型
	 * @return std::string 枚举名，若未找到返回 "ZOMBIE_NONE"
	 */
	std::string ZombieTypeToEnumName(ZombieType type) const;

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
	 * @brief 按类型创建植物（注册表派发，取代 GameApp 的 switch）
	 * @return 未注册或缺工厂返回 nullptr
	 */
	std::shared_ptr<Plant> CreatePlant(PlantType type, Board* board, int row, int col, bool isPreview) const;

	/**
	 * @brief 按类型创建僵尸（注册表派发）
	 * @return 未注册或缺工厂返回 nullptr
	 */
	std::shared_ptr<Zombie> CreateZombie(ZombieType type, Board* board, float x, float y, int row, bool isPreview) const;

	/**
	 * @brief 打印所有注册数据
	 */
	void DebugPrintAll() const;

private:
	GameDataManager();

	/**
	 * @brief 硬编码初始化所有植物和僵尸的身份数据（数值一律来自 gamedata.json）
	 */
	void InitializeHardcodedData();

	/**
	 * @brief 从 ./resources/gamedata.json 回填全部数值字段（JSON 是数值唯一来源）。
	 *        严格校验：文件缺失/解析失败/任一已注册类型缺条目/条目缺任一字段/类型错
	 *        → 收集全部错误统一 LOG_ERROR + 弹窗（AutoTest 模式不弹），返回 false。
	 *        JSON 中未注册的多余键仅 LOG_WARN 不阻断。
	 */
	bool LoadNumbersFromJson();

	/**
	 * @brief 注册一种植物的身份数据（内部使用）。数值（cost/cooldown/offset/scale）
	 *        一律来自 resources/gamedata.json，由 LoadNumbersFromJson 回填。
	 */
	void RegisterPlant(PlantType type,
		const std::string& enumName,
		const std::string& textureKey,
		AnimationType animType,
		const std::string& animName,
		PlantFactoryFn factory);

	/**
	 * @brief 注册一种僵尸的身份数据（内部使用）。数值（weight/appearWave/
	 *        survivalRound/offset/scale）一律来自 resources/gamedata.json。
	 */
	void RegisterZombie(ZombieType type,
		const std::string& enumName,
		AnimationType animType,
		const std::string& animName,
		ZombieFactoryFn factory);

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
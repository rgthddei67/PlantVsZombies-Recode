#include "GameDataManager.h"
#include "../../ResourceKeys.h"
#include <iostream>
#include <algorithm>
#include <vector>

GameDataManager::GameDataManager() {}

void GameDataManager::Initialize() {
	// 清空现有数据
	mPlantInfo.clear();
	mZombieInfo.clear();
	mAnimToString.clear();
	mEnumNameToType.clear();
	mTextureKeyToType.clear();
	mAnimNameToType.clear();

	InitializeHardcodedData();
#ifdef _DEBUG
	std::cout << "[GameDataManager] 初始化完成，共注册 "
		<< mPlantInfo.size() << " 种植物，"
		<< mZombieInfo.size() << " 种僵尸" << std::endl;
#endif
}

void GameDataManager::InitializeHardcodedData() {
	// ==================== 植物注册 ====================
	RegisterPlant(
		PlantType::PLANT_SUNFLOWER,
		50, 7.5f,
		"PLANT_SUNFLOWER",
		ResourceKeys::Textures::IMAGE_SUNFLOWER,
		AnimationType::ANIM_SUNFLOWER,
		ResourceKeys::Reanimations::REANIM_SUNFLOWER,
		Vector(-37.6f, -44)
	);

	RegisterPlant(
		PlantType::PLANT_PEASHOOTER,
		100, 7.5f,
		"PLANT_PEASHOOTER",
		ResourceKeys::Textures::IMAGE_PEASHOOTER,
		AnimationType::ANIM_PEASHOOTER,
		ResourceKeys::Reanimations::REANIM_PEASHOOTER,
		Vector(-37.6f, -44)
	);


	// ==================== 僵尸注册 ====================
	// 普通僵尸
	RegisterZombie(
		ZombieType::ZOMBIE_NORMAL,
		"ZOMBIE_NORMAL",			// 这个项目其实没啥意义，随便填
		AnimationType::ANIM_NORMAL_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_NORMAL_ZOMBIE,
		Vector(-50, -85),
		1000,
		1
	);

	RegisterZombie(
		ZombieType::ZOMBIE_TRAFFIC_CONE,
		"ZOMBIE_TRAFFIC_CONE",
		AnimationType::ANIM_CONE_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_CONE_ZOMBIE,
		Vector(-50, -85),
		1200,
		3
	);

	// ==================== 非植物/僵尸动画映射 ====================
	mAnimToString[AnimationType::ANIM_SUN] = ResourceKeys::Reanimations::REANIM_SUN;
	mAnimToString[AnimationType::ANIM_NONE] = "Unknown";
}

void GameDataManager::RegisterPlant(PlantType type,
	int sunCost, float cooldown,
	const std::string& enumName,
	const std::string& textureKey,
	AnimationType animType,
	const std::string& animName,
	const Vector& offset) {
	PlantInfo info(type, sunCost, cooldown, enumName, textureKey, animType, animName, offset);
	mPlantInfo[type] = info;

	mEnumNameToType[enumName] = type;
	mTextureKeyToType[textureKey] = type;
	mAnimNameToType[animName] = type;

	mAnimToString[animType] = animName;

#ifdef _DEBUG
	std::cout << "[GameDataManager] 注册植物: " << animName
		<< " (偏移: " << offset.x << ", " << offset.y << ")" << std::endl;
#endif
}

void GameDataManager::RegisterZombie(ZombieType type,
	const std::string& enumName,
	AnimationType animType,
	const std::string& animName,
	const Vector& offset, int weight, int appearWave) {
	ZombieInfo info(type, enumName, animType, animName, offset, weight, appearWave);
	mZombieInfo[type] = info;

	// 记录动画类型->资源名，以便通过 AnimationType 统一查询
	mAnimToString[animType] = animName;

#ifdef _DEBUG
	std::cout << "[GameDataManager] 注册僵尸: " << animName
		<< " (偏移: " << offset.x << ", " << offset.y << ")" << std::endl;
#endif
}

std::string GameDataManager::GetPlantTextureKey(PlantType plantType) const {
	auto it = mPlantInfo.find(plantType);
	if (it != mPlantInfo.end())
		return it->second.textureKey;
	return "IMAGE_PLANT_DEFAULT";
}

AnimationType GameDataManager::GetPlantAnimationType(PlantType plantType) const {
	auto it = mPlantInfo.find(plantType);
	if (it != mPlantInfo.end())
		return it->second.animType;
	return AnimationType::ANIM_NONE;
}

std::string GameDataManager::GetAnimationName(AnimationType animType) const {
	// 优先在植物动画中查找
	for (const auto& pair : mPlantInfo) {
		if (pair.second.animType == animType)
			return pair.second.animName;
	}
	// 然后在僵尸动画中查找
	for (const auto& pair : mZombieInfo) {
		if (pair.second.animType == animType)
			return pair.second.animName;
	}
	// 最后在通用映射中查找
	auto it = mAnimToString.find(animType);
	if (it != mAnimToString.end())
		return it->second;
	return "Unknown";
}

Vector GameDataManager::GetPlantOffset(PlantType plantType) const {
	auto it = mPlantInfo.find(plantType);
	if (it != mPlantInfo.end())
		return it->second.offset;
	return Vector(0, 0);
}

void GameDataManager::SetPlantOffset(PlantType plantType, const Vector& offset) {
	auto it = mPlantInfo.find(plantType);
	if (it != mPlantInfo.end()) {
		it->second.offset = offset;
#ifdef _DEBUG
		std::cout << "[GameDataManager] 设置植物偏移: " << it->second.animName
			<< " -> (" << offset.x << ", " << offset.y << ")" << std::endl;
#endif
	}
}

std::string GameDataManager::PlantTypeToEnumName(PlantType type) const {
	auto it = mPlantInfo.find(type);
	if (it != mPlantInfo.end())
		return it->second.enumName;
	return "PLANT_NONE";
}

std::string GameDataManager::PlantTypeToTextureKey(PlantType type) const {
	return GetPlantTextureKey(type);
}

std::string GameDataManager::PlantTypeToAnimName(PlantType type) const {
	auto it = mPlantInfo.find(type);
	if (it != mPlantInfo.end())
		return it->second.animName;
	return "Unknown";
}

PlantType GameDataManager::StringToPlantType(const std::string& str) const {
	auto enumIt = mEnumNameToType.find(str);
	if (enumIt != mEnumNameToType.end())
		return enumIt->second;

	auto texIt = mTextureKeyToType.find(str);
	if (texIt != mTextureKeyToType.end())
		return texIt->second;

	auto animIt = mAnimNameToType.find(str);
	if (animIt != mAnimNameToType.end())
		return animIt->second;

	return PlantType::NUM_PLANT_TYPES;
}

std::vector<PlantType> GameDataManager::GetAllPlantTypes() const {
	std::vector<PlantType> types;
	for (const auto& pair : mPlantInfo)
		types.push_back(pair.first);
	return types;
}

bool GameDataManager::HasPlant(PlantType type) const {
	return mPlantInfo.find(type) != mPlantInfo.end();
}

int GameDataManager::GetPlantSunCost(PlantType plantType) const {
	auto it = mPlantInfo.find(plantType);
	if (it != mPlantInfo.end())
		return it->second.SunCost;
	return 0;
}

float GameDataManager::GetPlantCooldown(PlantType plantType) const {
	auto it = mPlantInfo.find(plantType);
	if (it != mPlantInfo.end())
		return it->second.Cooldown;
	return 0.0f;
}

AnimationType GameDataManager::GetZombieAnimationType(ZombieType zombieType) const {
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end())
		return it->second.animType;
	return AnimationType::ANIM_NONE;
}

std::string GameDataManager::GetZombieAnimName(ZombieType zombieType) const {
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end())
		return it->second.animName;
	return "Unknown";
}

Vector GameDataManager::GetZombieOffset(ZombieType zombieType) const {
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end())
		return it->second.offset;
	return Vector(0, 0);
}

void GameDataManager::SetZombieOffset(ZombieType zombieType, const Vector& offset) {
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end()) {
		it->second.offset = offset;
#ifdef _DEBUG
		std::cout << "[GameDataManager] 设置僵尸偏移: " << it->second.animName
			<< " -> (" << offset.x << ", " << offset.y << ")" << std::endl;
#endif
	}
}

int GameDataManager::GetZombieWeight(ZombieType zombieType) const
{
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end())
		return it->second.weight;
	return 0;
}

int GameDataManager::GetZombieAppearWave(ZombieType zombieType) const
{
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end())
		return it->second.appearWave;
	return 0;
}

std::vector<ZombieType> GameDataManager::GetAllZombieTypes() const {
	std::vector<ZombieType> types;
	for (const auto& pair : mZombieInfo)
		types.push_back(pair.first);
	return types;
}

bool GameDataManager::HasZombie(ZombieType type) const {
	return mZombieInfo.find(type) != mZombieInfo.end();
}

void GameDataManager::DebugPrintAll() const {
	std::cout << "========== GameDataManager 硬编码数据 ==========" << std::endl;
	std::cout << "植物总数: " << mPlantInfo.size() << std::endl;
	for (const auto& pair : mPlantInfo) {
		const PlantInfo& info = pair.second;
		std::cout << "植物: " << info.animName << " (" << info.enumName << ")" << std::endl;
		std::cout << "  纹理键: " << info.textureKey << std::endl;
		std::cout << "  动画类型: " << static_cast<int>(info.animType) << std::endl;
		std::cout << "  偏移量: (" << info.offset.x << ", " << info.offset.y << ")" << std::endl;
	}

	std::cout << "\n僵尸总数: " << mZombieInfo.size() << std::endl;
	for (const auto& pair : mZombieInfo) {
		const ZombieInfo& info = pair.second;
		std::cout << "僵尸: " << info.animName << " (" << info.enumName << ")" << std::endl;
		std::cout << "  动画类型: " << static_cast<int>(info.animType) << std::endl;
		std::cout << "  偏移量: (" << info.offset.x << ", " << info.offset.y << ")" << std::endl;
	}
	std::cout << "=============================================" << std::endl;
}
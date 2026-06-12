#include "GameDataManager.h"
#include "../../ResourceKeys.h"
#include "../../Logger.h"
#include <algorithm>
#include <vector>

#include "../GameObjectManager.h"
#include "../RenderOrder.h"
#include "../Board.h"

#include "PeaShooter.h"
#include "SunFlower.h"
#include "CherryBomb.h"
#include "WallNut.h"
#include "PotatoMine.h"
#include "SnowPeaShooter.h"
#include "Chomper.h"
#include "Repeater.h"
#include "PuffShroom.h"

#include "../Zombie/Zombie.h"
#include "../Zombie/ConeZombie.h"
#include "../Zombie/Polevaulter.h"
#include "../Zombie/BucketZombie.h"
#include "../Zombie/FastBucketZombie.h"
#include "../Zombie/PaperZombie.h"
#include "../Zombie/FastPaperZombie.h"

namespace {
	template<typename T>
	std::shared_ptr<Plant> MakePlant(Board* b, PlantType t, int row, int col,
		AnimationType anim, float scale, bool preview) {
		return GameObjectManager::GetInstance()
			.CreateGameObjectImmediateAsShared<T>(LAYER_GAME_PLANT, b, t, row, col, anim, scale, preview);
	}
	template<typename T>
	std::shared_ptr<Zombie> MakeZombie(Board* b, ZombieType t, float x, float y, int row,
		AnimationType anim, float scale, bool preview) {
		return GameObjectManager::GetInstance()
			.CreateGameObjectImmediateAsShared<T>(LAYER_GAME_ZOMBIE, b, t, x, y, row, anim, scale, preview);
	}
}

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
	LOG_INFO("GameData") << "初始化完成，共注册 "
		<< mPlantInfo.size() << " 种植物，"
		<< mZombieInfo.size() << " 种僵尸";
}

void GameDataManager::InitializeHardcodedData() {
	// ============================================================
	// 【新增植物】共 4 步（不再需要改 GameApp 的 switch —— 已用注册式工厂取代）：
	//   1. PlantType.h    加枚举项
	//   2. Game/Plant/    写植物类（纯头文件即可，构造经 using 继承 Plant/Shooter）
	//   3. 本文件          在下方“植物注册”区加一行 RegisterPlant(..., scale, &MakePlant<新类>)
	//                      —— 含全部数据（cost/cooldown/纹理/动画/偏移/缩放）与构造工厂
	//      并在本文件顶部 #include "你的植物.h"
	//   4. 加 Card 条目
	// 【新增僵尸】同理：ZombieType.h 加枚举 → 写僵尸类 → 下方“僵尸注册”区加一行
	//      RegisterZombie(..., scale, &MakeZombie<新类>) + 顶部 #include；Board 波次逻辑按需。
	// 配套常量：动画类型加在 Reanimation/AnimationTypes.h；资源键加在 ResourceKeys.h。
	// ============================================================

	// ==================== 植物注册 ====================
	RegisterPlant(
		PlantType::PLANT_SUNFLOWER,
		50, 7.5f,
		"PLANT_SUNFLOWER",
		ResourceKeys::Textures::IMAGE_SUNFLOWER,
		AnimationType::ANIM_SUNFLOWER,
		ResourceKeys::Reanimations::REANIM_SUNFLOWER,
		Vector(-37.6f, -44),
		1.0f, &MakePlant<SunFlower>
	);

	RegisterPlant(
		PlantType::PLANT_PEASHOOTER,
		100, 7.5f,
		"PLANT_PEASHOOTER",
		ResourceKeys::Textures::IMAGE_PEASHOOTER,
		AnimationType::ANIM_PEASHOOTER,
		ResourceKeys::Reanimations::REANIM_PEASHOOTER,
		Vector(-37.6f, -44),
		1.0f, &MakePlant<PeaShooter>
	);

	RegisterPlant(
		PlantType::PLANT_CHERRYBOMB,
		150, 50.0f,
		"PLANT_CHERRYBOMB",
		ResourceKeys::Textures::IMAGE_CHERRYBOMB,
		AnimationType::ANIM_CHERRYBOMB,
		ResourceKeys::Reanimations::REANIM_CHERRYBOMB,
		Vector(-37.6f, -44),
		1.0f, &MakePlant<CherryBomb>
	);

	RegisterPlant(
		PlantType::PLANT_WALLNUT,
		50, 25.0f,
		"PLANT_WALLNUT",
		ResourceKeys::Textures::IMAGE_WALLNUT,
		AnimationType::ANIM_WALLNUT,
		ResourceKeys::Reanimations::REANIM_WALLNUT,
		Vector(-37.6f, -44),
		1.0f, &MakePlant<WallNut>
	);

	RegisterPlant(
		PlantType::PLANT_POTATOMINE,
		25, 20.0f,
		"PLANT_POTATOMINE",
		ResourceKeys::Textures::IMAGE_POTATOMINE,
		AnimationType::ANIM_POTATOMINE,
		ResourceKeys::Reanimations::REANIM_POTATOMINE,
		Vector(-30.0f, -35.2f),
		0.8f, &MakePlant<PotatoMine>
	);

	RegisterPlant(
		PlantType::PLANT_SNOWPEA,
		175, 7.5f,
		"PLANT_SNOWPEASHOOTER",
		ResourceKeys::Textures::IMAGE_SNOWPEASHOOTER,
		AnimationType::ANIM_SNOWPEASHOOTER,
		ResourceKeys::Reanimations::REANIM_SNOWPEASHOOTER,
		Vector(-37.6f, -44),
		1.0f, &MakePlant<SnowPeaShooter>
	);

	RegisterPlant(
		PlantType::PLANT_CHOMPER,
		150, 7.5f,
		"PLANT_CHOMPER",
		ResourceKeys::Textures::IMAGE_CHOMPER,
		AnimationType::ANIM_CHOMPER,
		"Chomper",
		Vector(-37.6f, -44),
		1.0f, &MakePlant<Chomper>
	);

	RegisterPlant(
		PlantType::PLANT_REPEATER,
		200, 7.5f,
		"PLANT_REPEATER",
		ResourceKeys::Textures::IMAGE_REPEATER,
		AnimationType::ANIM_REPEAT,
		"Repeater",
		Vector(-37.6f, -44),
		1.0f, &MakePlant<Repeater>
	);

	RegisterPlant(
		PlantType::PLANT_PUFFSHROOM,
		0, 7.5f,
		"PLANT_PUFFSHROOM",
		ResourceKeys::Textures::IMAGE_PUFFSHROOM,
		AnimationType::ANIM_PUFFSHROOM,
		"PuffShroom",
		Vector(-37.6f, -28),
		1.0f, &MakePlant<PuffShroom>
	);

	// ==================== 僵尸注册 ====================
	// 普通僵尸
	RegisterZombie(
		ZombieType::ZOMBIE_NORMAL,
		"ZOMBIE_NORMAL",
		AnimationType::ANIM_NORMAL_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_NORMAL_ZOMBIE,
		Vector(-50, -85),
		1000,
		1,
		1.0f, &MakeZombie<Zombie>
	);

	RegisterZombie(
		ZombieType::ZOMBIE_TRAFFIC_CONE,
		"ZOMBIE_TRAFFIC_CONE",
		AnimationType::ANIM_CONE_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_CONE_ZOMBIE,
		Vector(-50, -85),
		1500,
		3,
		1.0f, &MakeZombie<ConeZombie>
	);

	RegisterZombie(
		ZombieType::ZOMBIE_POLEVAULTER,
		"ZOMBIE_POLEVAULTER",
		AnimationType::ANIM_POLEVAULTER_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_POLEVAULTER_ZOMBIE,
		Vector(-50, -85),
		1700,
		5,
		1.0f, &MakeZombie<Polevaulter>
	);

	RegisterZombie(
		ZombieType::ZOMBIE_BUCKET,
		"ZOMBIE_BUCKET",
		AnimationType::ANIM_BUCKET_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_BUCKET_ZOMBIE,
		Vector(-50, -85),
		2000,
		5,
		1.0f, &MakeZombie<BucketZombie>
	);

	RegisterZombie(
		ZombieType::ZOMBIE_FASTBUCKET,
		"ZOMBIE_FASTBUCKET",
		AnimationType::ANIM_BUCKET_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_BUCKET_ZOMBIE,
		Vector(-50, -85),
		2500,
		6,
		1.0f, &MakeZombie<FastBucketZombie>
	);

	RegisterZombie(
		ZombieType::ZOMBIE_NEWSPAPER,
		"ZOMBIE_NEWSPAPER",
		AnimationType::ANIM_PAPER_ZOMBIE,
		"PaperZombie",
		Vector(-50, -85),
		2000,
		2,
		1.0f, &MakeZombie<PaperZombie>
	);

	RegisterZombie(
		ZombieType::ZOMBIE_FASTPAPER,
		"ZOMBIE_FASTPAPER",
		AnimationType::ANIM_PAPER_ZOMBIE,	// 复用读报僵尸 reanim，仅换报纸贴图
		"PaperZombie",
		Vector(-50, -85),
		2800,	// 权重/点数成本：略高于快速铁桶(2500)，体现加强版
		6,		// 出现波次（无尽模式忽略此值，由 BuildSurvivalSpawnList 控制）
		1.0f, &MakeZombie<FastPaperZombie>
	);

	// ==================== 非植物/僵尸动画映射 ====================
	mAnimToString[AnimationType::ANIM_SUN] = ResourceKeys::Reanimations::REANIM_SUN;

	mAnimToString[AnimationType::ANIM_ZOMBIE_CHARRED] =
		ResourceKeys::Reanimations::REANIM_ZOMBIE_CHARRED;

	mAnimToString[AnimationType::ANIM_LAWNMOWER] =
		ResourceKeys::Reanimations::REANIM_LAWNMOWER;

	mAnimToString[AnimationType::ANIM_NONE] = "Unknown";
}

void GameDataManager::RegisterPlant(PlantType type,
	int sunCost, float cooldown,
	const std::string& enumName,
	const std::string& textureKey,
	AnimationType animType,
	const std::string& animName,
	const Vector& offset,
	float scale,
	PlantFactoryFn factory) {
	PlantInfo info(type, sunCost, cooldown, enumName, textureKey, animType, animName, offset);
	info.scale = scale;
	info.factory = factory;
	mPlantInfo[type] = info;

	mEnumNameToType[enumName] = type;
	mTextureKeyToType[textureKey] = type;
	mAnimNameToType[animName] = type;

	mAnimToString[animType] = animName;

	LOG_DEBUG("GameData") << "注册植物: " << animName
		<< " (偏移: " << offset.x << ", " << offset.y << ")";
}

void GameDataManager::RegisterZombie(ZombieType type,
	const std::string& enumName,
	AnimationType animType,
	const std::string& animName,
	const Vector& offset, int weight, int appearWave,
	float scale,
	ZombieFactoryFn factory) {
	ZombieInfo info(type, enumName, animType, animName, offset, weight, appearWave);
	info.scale = scale;
	info.factory = factory;
	mZombieInfo[type] = info;

	// 记录动画类型->资源名，以便通过 AnimationType 统一查询
	mAnimToString[animType] = animName;

	LOG_DEBUG("GameData") << "注册僵尸: " << animName
		<< " (偏移: " << offset.x << ", " << offset.y << ")";
}

std::shared_ptr<Plant> GameDataManager::CreatePlant(PlantType type, Board* board, int row, int col, bool isPreview) const {
	auto it = mPlantInfo.find(type);
	if (it == mPlantInfo.end() || !it->second.factory) {
		LOG_ERROR("GameData") << "未注册或缺工厂的植物类型: " << static_cast<int>(type);
		return nullptr;
	}
	const PlantInfo& i = it->second;
	return i.factory(board, type, row, col, i.animType, i.scale, isPreview);
}

std::shared_ptr<Zombie> GameDataManager::CreateZombie(ZombieType type, Board* board, float x, float y, int row, bool isPreview) const {
	auto it = mZombieInfo.find(type);
	if (it == mZombieInfo.end() || !it->second.factory) {
		LOG_ERROR("GameData") << "未注册或缺工厂的僵尸类型: " << static_cast<int>(type);
		return nullptr;
	}
	const ZombieInfo& i = it->second;
	return i.factory(board, type, x, y, row, i.animType, i.scale, isPreview);
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
		LOG_DEBUG("GameData") << "设置植物偏移: " << it->second.animName
			<< " -> (" << offset.x << ", " << offset.y << ")";
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
		LOG_DEBUG("GameData") << "设置僵尸偏移: " << it->second.animName
			<< " -> (" << offset.x << ", " << offset.y << ")";
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

std::string GameDataManager::ZombieTypeToEnumName(ZombieType type) const {
	auto it = mZombieInfo.find(type);
	if (it != mZombieInfo.end())
		return it->second.enumName;
	return "ZOMBIE_NONE";
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
	LOG_DEBUG("GameData") << "========== GameDataManager 硬编码数据 ==========";
	LOG_DEBUG("GameData") << "植物总数: " << mPlantInfo.size();
	for (const auto& pair : mPlantInfo) {
		const PlantInfo& info = pair.second;
		LOG_DEBUG("GameData") << "植物: " << info.animName << " (" << info.enumName << ")";
		LOG_DEBUG("GameData") << "  纹理键: " << info.textureKey;
		LOG_DEBUG("GameData") << "  动画类型: " << static_cast<int>(info.animType);
		LOG_DEBUG("GameData") << "  偏移量: (" << info.offset.x << ", " << info.offset.y << ")";
	}

	LOG_DEBUG("GameData") << "僵尸总数: " << mZombieInfo.size();
	for (const auto& pair : mZombieInfo) {
		const ZombieInfo& info = pair.second;
		LOG_DEBUG("GameData") << "僵尸: " << info.animName << " (" << info.enumName << ")";
		LOG_DEBUG("GameData") << "  动画类型: " << static_cast<int>(info.animType);
		LOG_DEBUG("GameData") << "  偏移量: (" << info.offset.x << ", " << info.offset.y << ")";
	}
	LOG_DEBUG("GameData") << "=============================================";
}
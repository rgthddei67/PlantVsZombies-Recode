#include "GameDataManager.h"
#include "../../ResourceKeys.h"
#include "../../Logger.h"
#include "../../FileManager.h"
#include "../AutoTest/TestDriver.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <vector>
#include <unordered_set>

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
#include "SunShroom.h"
#include "FumeShroom.h"
#include "IceFumeShroom.h"
#include "HypnoShroom.h"
#include "ScaredyShroom.h"
#include "IceShroom.h"
#include "DoomShroom.h"

#include "../Zombie/Zombie.h"
#include "../Zombie/ConeZombie.h"
#include "../Zombie/Polevaulter.h"
#include "../Zombie/BucketZombie.h"
#include "../Zombie/FastBucketZombie.h"
#include "../Zombie/PaperZombie.h"
#include "../Zombie/FastPaperZombie.h"
#include "../Zombie/DoorZombie.h"
#include "../Zombie/FootballZombie.h"
#include "../Zombie/PinkFootballZombie.h"
#include "../Zombie/DancerZombie.h"
#include "../Zombie/BackupDancerZombie.h"
#include "../Zombie/EliteDancerZombie.h"
#include "../Zombie/ReinforcedDoorZombie.h"

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

bool GameDataManager::Initialize() {
	// 清空现有数据
	mPlantInfo.clear();
	mZombieInfo.clear();
	mAnimToString.clear();
	mEnumNameToType.clear();
	mTextureKeyToType.clear();
	mAnimNameToType.clear();

	InitializeHardcodedData();
	if (!LoadNumbersFromJson()) return false;

	LOG_INFO("GameData") << "初始化完成，共注册 "
		<< mPlantInfo.size() << " 种植物，"
		<< mZombieInfo.size() << " 种僵尸（数值来自 gamedata.json）";
	return true;
}

void GameDataManager::InitializeHardcodedData() {
	// ============================================================
	// 本函数只注册【身份数据】：枚举名/纹理键/动画/构造工厂。
	// 全部【数值】（cost/cooldown/weight/appearWave/survivalRound/offset/scale）
	// 住在 resources/gamedata.json（数值唯一来源，改数值不用重编译）。
	// 【新增植物】共 5 步（不再需要改 GameApp 的 switch —— 已用注册式工厂取代）：
	//   1. PlantType.h    加枚举项
	//   2. Game/Plant/    写植物类（纯头文件即可，构造经 using 继承 Plant/Shooter）
	//   3. 本文件          在下方“植物注册”区加一行 RegisterPlant(..., &MakePlant<新类>)
	//      并在本文件顶部 #include "你的植物.h"
	//   4. resources/gamedata.json 的 "plants" 加同枚举名条目（漏了 → 启动时报错退出，
	//      注意资源目录在 build/<preset>/resources/，两个 preset 都要加）
	//   5. 加 Card 条目
	// 【新增僵尸】同理：ZombieType.h 加枚举 → 写僵尸类 → 下方“僵尸注册”区加一行
	//      RegisterZombie(..., &MakeZombie<新类>) + 顶部 #include
	//      + gamedata.json 的 "zombies" 加条目（weight/appearWave/survivalRound 都在 JSON）。
	// 配套常量：动画类型加在 Reanimation/AnimationTypes.h；资源键加在 ResourceKeys.h。
	// ============================================================

	// ==================== 植物注册（仅身份，数值见 gamedata.json） ====================
	RegisterPlant(PlantType::PLANT_SUNFLOWER, "PLANT_SUNFLOWER",
		ResourceKeys::Textures::IMAGE_SUNFLOWER,
		AnimationType::ANIM_SUNFLOWER,
		ResourceKeys::Reanimations::REANIM_SUNFLOWER, &MakePlant<SunFlower>);

	RegisterPlant(PlantType::PLANT_PEASHOOTER, "PLANT_PEASHOOTER",
		ResourceKeys::Textures::IMAGE_PEASHOOTER,
		AnimationType::ANIM_PEASHOOTER,
		ResourceKeys::Reanimations::REANIM_PEASHOOTER, &MakePlant<PeaShooter>);

	RegisterPlant(PlantType::PLANT_CHERRYBOMB, "PLANT_CHERRYBOMB",
		ResourceKeys::Textures::IMAGE_CHERRYBOMB,
		AnimationType::ANIM_CHERRYBOMB,
		ResourceKeys::Reanimations::REANIM_CHERRYBOMB, &MakePlant<CherryBomb>);

	RegisterPlant(PlantType::PLANT_WALLNUT, "PLANT_WALLNUT",
		ResourceKeys::Textures::IMAGE_WALLNUT,
		AnimationType::ANIM_WALLNUT,
		ResourceKeys::Reanimations::REANIM_WALLNUT, &MakePlant<WallNut>);

	RegisterPlant(PlantType::PLANT_POTATOMINE, "PLANT_POTATOMINE",
		ResourceKeys::Textures::IMAGE_POTATOMINE,
		AnimationType::ANIM_POTATOMINE,
		ResourceKeys::Reanimations::REANIM_POTATOMINE, &MakePlant<PotatoMine>);

	RegisterPlant(PlantType::PLANT_SNOWPEA, "PLANT_SNOWPEASHOOTER",
		ResourceKeys::Textures::IMAGE_SNOWPEASHOOTER,
		AnimationType::ANIM_SNOWPEASHOOTER,
		ResourceKeys::Reanimations::REANIM_SNOWPEASHOOTER, &MakePlant<SnowPeaShooter>);

	RegisterPlant(PlantType::PLANT_CHOMPER, "PLANT_CHOMPER",
		ResourceKeys::Textures::IMAGE_CHOMPER,
		AnimationType::ANIM_CHOMPER,
		"Chomper", &MakePlant<Chomper>);

	RegisterPlant(PlantType::PLANT_REPEATER, "PLANT_REPEATER",
		ResourceKeys::Textures::IMAGE_REPEATER,
		AnimationType::ANIM_REPEAT,
		"Repeater", &MakePlant<Repeater>);

	RegisterPlant(PlantType::PLANT_PUFFSHROOM, "PLANT_PUFFSHROOM",
		ResourceKeys::Textures::IMAGE_PUFFSHROOM,
		AnimationType::ANIM_PUFFSHROOM,
		"PuffShroom", &MakePlant<PuffShroom>);

	RegisterPlant(PlantType::PLANT_SUNSHROOM, "PLANT_SUNSHROOM",
		ResourceKeys::Textures::IMAGE_SUNSHROOM,
		AnimationType::ANIM_SUNSHROOM,
		"SunShroom", &MakePlant<SunShroom>);

	RegisterPlant(PlantType::PLANT_FUMESHROOM, "PLANT_FUMESHROOM",
		ResourceKeys::Textures::IMAGE_FUMESHROOM,
		AnimationType::ANIM_FUMESHROOM,
		"FumeShroom", &MakePlant<FumeShroom>);

	RegisterPlant(PlantType::PLANT_HYPNOSHROOM, "PLANT_HYPNOSHROOM",
		ResourceKeys::Textures::IMAGE_HYPNOSHROOM,
		AnimationType::ANIM_HYPER,
		"HypnoShroom", &MakePlant<HypnoShroom>);

	RegisterPlant(PlantType::PLANT_SCAREDYSHROOM, "PLANT_SCAREDYSHROOM",
		ResourceKeys::Textures::IMAGE_SCAREDYSHROOM,
		AnimationType::ANIM_SCAREDYSHROOM,
		"ScaredyShroom", &MakePlant<ScaredyShroom>);

	RegisterPlant(PlantType::PLANT_ICESHROOM, "PLANT_ICESHROOM",
		ResourceKeys::Textures::IMAGE_ICESHROOM,
		AnimationType::ANIM_ICE_SHROOM,
		"IceShroom", &MakePlant<IceShroom>);

	RegisterPlant(PlantType::PLANT_DOOMSHROOM, "PLANT_DOOMSHROOM",
		ResourceKeys::Textures::IMAGE_DOOMSHROOM,
		AnimationType::ANIM_DOOM_SHROOM,
		"DoomShroom", &MakePlant<DoomShroom>);

	// 寒冰大喷菇：复用大喷菇 reanim（蓝色靠 overlay），仅卡图独立
	RegisterPlant(PlantType::PLANT_ICEFUMESHROOM, "PLANT_ICEFUMESHROOM",
		ResourceKeys::Textures::IMAGE_ICEFUMESHROOM,
		AnimationType::ANIM_ICEFUMESHROOM,
		"FumeShroom", &MakePlant<IceFumeShroom>);

	// ==================== 僵尸注册（仅身份，数值见 gamedata.json） ====================
	RegisterZombie(ZombieType::ZOMBIE_NORMAL, "ZOMBIE_NORMAL",
		AnimationType::ANIM_NORMAL_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_NORMAL_ZOMBIE, &MakeZombie<Zombie>);

	RegisterZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, "ZOMBIE_TRAFFIC_CONE",
		AnimationType::ANIM_CONE_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_CONE_ZOMBIE, &MakeZombie<ConeZombie>);

	RegisterZombie(ZombieType::ZOMBIE_POLEVAULTER, "ZOMBIE_POLEVAULTER",
		AnimationType::ANIM_POLEVAULTER_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_POLEVAULTER_ZOMBIE, &MakeZombie<Polevaulter>);

	RegisterZombie(ZombieType::ZOMBIE_BUCKET, "ZOMBIE_BUCKET",
		AnimationType::ANIM_BUCKET_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_BUCKET_ZOMBIE, &MakeZombie<BucketZombie>);

	RegisterZombie(ZombieType::ZOMBIE_FASTBUCKET, "ZOMBIE_FASTBUCKET",
		AnimationType::ANIM_BUCKET_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_BUCKET_ZOMBIE, &MakeZombie<FastBucketZombie>);

	RegisterZombie(ZombieType::ZOMBIE_NEWSPAPER, "ZOMBIE_NEWSPAPER",
		AnimationType::ANIM_PAPER_ZOMBIE,
		"PaperZombie", &MakeZombie<PaperZombie>);

	// 复用读报僵尸 reanim，仅换报纸贴图
	RegisterZombie(ZombieType::ZOMBIE_FASTPAPER, "ZOMBIE_FASTPAPER",
		AnimationType::ANIM_PAPER_ZOMBIE,
		"PaperZombie", &MakeZombie<FastPaperZombie>);

	RegisterZombie(ZombieType::ZOMBIE_DOOR, "ZOMBIE_DOOR",
		AnimationType::ANIM_DOOR_ZOMBIE,
		"DoorZombie", &MakeZombie<DoorZombie>);

	RegisterZombie(ZombieType::ZOMBIE_FOOTBALL, "ZOMBIE_FOOTBALL",
		AnimationType::ANIM_FOOTBALL_ZOMBIE,
		"FootballZombie", &MakeZombie<FootballZombie>);

	RegisterZombie(ZombieType::ZOMBIE_PINK_FOOTBALL, "ZOMBIE_PINK_FOOTBALL",
		AnimationType::ANIM_PINK_FOOTBALL_ZOMBIE,
		"PinkFootballZombie", &MakeZombie<PinkFootballZombie>);

	// 舞王僵尸（MJ版）：入场后打响指召唤十字 4 伴舞，死伴舞按节拍补位
	RegisterZombie(ZombieType::ZOMBIE_DANCER, "ZOMBIE_DANCER",
		AnimationType::ANIM_DANCE_ZOMBIE,
		"ZombieJackson", &MakeZombie<DancerZombie>);

	// 伴舞僵尸：gamedata weight=0，只能被舞王召唤（或 AutoTest spawn_zombie 直造）
	RegisterZombie(ZombieType::ZOMBIE_BACKUP_DANCER, "ZOMBIE_BACKUP_DANCER",
		AnimationType::ANIM_DANCERWITH_ZOMBIE,
		"ZombieDancer", &MakeZombie<BackupDancerZombie>);

	// 精英舞王：正式波次仅由强台风以上天气把普通舞王变异为该类型。
	RegisterZombie(ZombieType::ZOMBIE_ELITE_DANCER, "ZOMBIE_ELITE_DANCER",
		AnimationType::ANIM_ELITE_DANCE_ZOMBIE,
		ResourceKeys::Reanimations::REANIM_ZOMBIE_ELITE_JACKSON,
		&MakeZombie<EliteDancerZombie>);

	// 加固铁门：复用铁门动画，派生类在 SetupZombie 内替换门材质并覆盖耐久规则。
	RegisterZombie(ZombieType::ZOMBIE_REINFORCED_DOOR, "ZOMBIE_REINFORCED_DOOR",
		AnimationType::ANIM_DOOR_ZOMBIE,
		"DoorZombie", &MakeZombie<ReinforcedDoorZombie>);

	// ==================== 非植物/僵尸动画映射 ====================
	mAnimToString[AnimationType::ANIM_SUN] = ResourceKeys::Reanimations::REANIM_SUN;

	mAnimToString[AnimationType::ANIM_ZOMBIE_CHARRED] =
		ResourceKeys::Reanimations::REANIM_ZOMBIE_CHARRED;

	mAnimToString[AnimationType::ANIM_LAWNMOWER] =
		ResourceKeys::Reanimations::REANIM_LAWNMOWER;

	mAnimToString[AnimationType::ANIM_NONE] = "Unknown";
}

void GameDataManager::RegisterPlant(PlantType type,
	const std::string& enumName,
	const std::string& textureKey,
	AnimationType animType,
	const std::string& animName,
	PlantFactoryFn factory) {
	PlantInfo info;
	info.type = type;
	info.enumName = enumName;
	info.textureKey = textureKey;
	info.animType = animType;
	info.animName = animName;
	info.factory = factory;
	mPlantInfo[type] = info;

	mEnumNameToType[enumName] = type;
	mTextureKeyToType[textureKey] = type;
	mAnimNameToType[animName] = type;
	mAnimToString[animType] = animName;

	LOG_DEBUG("GameData") << "注册植物身份: " << enumName;
}

void GameDataManager::RegisterZombie(ZombieType type,
	const std::string& enumName,
	AnimationType animType,
	const std::string& animName,
	ZombieFactoryFn factory) {
	ZombieInfo info;
	info.type = type;
	info.enumName = enumName;
	info.animType = animType;
	info.animName = animName;
	info.factory = factory;
	mZombieInfo[type] = info;

	// 记录动画类型->资源名，以便通过 AnimationType 统一查询
	mAnimToString[animType] = animName;

	LOG_DEBUG("GameData") << "注册僵尸身份: " << enumName;
}

bool GameDataManager::LoadNumbersFromJson() {
	std::vector<std::string> errors;
	nlohmann::json data;
	if (!FileManager::LoadJsonFile("./resources/gamedata.json", data)) {
		errors.push_back("resources/gamedata.json 缺失或解析失败");
	}
	else {
		// 单字段读取器：缺失/类型不符记错（不中断，攒齐所有错误一次报告）
		auto readInt = [&errors](const nlohmann::json& e, const char* field,
			const std::string& who, int& out) {
			if (!e.contains(field) || !e[field].is_number_integer()) {
				errors.push_back(who + " 缺字段 \"" + field + "\"（须为整数）");
				return;
			}
			out = e[field].get<int>();
		};
		auto readFloat = [&errors](const nlohmann::json& e, const char* field,
			const std::string& who, float& out) {
			if (!e.contains(field) || !e[field].is_number()) {
				errors.push_back(who + " 缺字段 \"" + field + "\"（须为数字）");
				return;
			}
			out = e[field].get<float>();
		};
		auto readOffset = [&errors](const nlohmann::json& e,
			const std::string& who, Vector& out) {
			if (!e.contains("offset") || !e["offset"].is_array() || e["offset"].size() != 2
				|| !e["offset"][0].is_number() || !e["offset"][1].is_number()) {
				errors.push_back(who + " 缺字段 \"offset\"（须为双元素数字数组）");
				return;
			}
			out = Vector(e["offset"][0].get<float>(), e["offset"][1].get<float>());
		};

		const bool hasPlants = data.contains("plants") && data["plants"].is_object();
		const bool hasZombies = data.contains("zombies") && data["zombies"].is_object();
		if (!hasPlants) errors.push_back("缺 \"plants\" 对象");
		if (!hasZombies) errors.push_back("缺 \"zombies\" 对象");

		if (hasPlants) {
			const nlohmann::json& plants = data["plants"];
			for (auto& pair : mPlantInfo) {
				PlantInfo& info = pair.second;
				if (!plants.contains(info.enumName)) {
					errors.push_back("缺 " + info.enumName + " 的条目");
					continue;
				}
				const nlohmann::json& e = plants[info.enumName];
				readInt(e, "cost", info.enumName, info.SunCost);
				readFloat(e, "cooldown", info.enumName, info.Cooldown);
				readOffset(e, info.enumName, info.offset);
				readFloat(e, "scale", info.enumName, info.scale);
			}
			for (auto& item : plants.items()) {
				if (mEnumNameToType.find(item.key()) == mEnumNameToType.end())
					LOG_WARN("GameData") << "gamedata.json 有未注册的植物键: " << item.key();
			}
		}

		if (hasZombies) {
			const nlohmann::json& zombies = data["zombies"];
			std::unordered_set<std::string> registeredNames;
			for (auto& pair : mZombieInfo) {
				ZombieInfo& info = pair.second;
				registeredNames.insert(info.enumName);
				if (!zombies.contains(info.enumName)) {
					errors.push_back("缺 " + info.enumName + " 的条目");
					continue;
				}
				const nlohmann::json& e = zombies[info.enumName];
				readInt(e, "weight", info.enumName, info.weight);
				readInt(e, "appearWave", info.enumName, info.appearWave);
				readInt(e, "survivalRound", info.enumName, info.survivalRound);
				readOffset(e, info.enumName, info.offset);
				readFloat(e, "scale", info.enumName, info.scale);
			}
			for (auto& item : zombies.items()) {
				if (registeredNames.find(item.key()) == registeredNames.end())
					LOG_WARN("GameData") << "gamedata.json 有未注册的僵尸键: " << item.key();
			}
		}
	}

	if (errors.empty()) return true;

	std::string all;
	for (const auto& msg : errors) {
		LOG_ERROR("GameData") << "gamedata.json: " << msg;
		all += msg;
		all += "\n";
	}
	// AutoTest/无头运行不弹模态框（否则负向测试卡死拿不到退出码）
	if (!TestDriver::GetInstance().IsActive()) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
			"gamedata.json 配置错误", all.c_str(), nullptr);
	}
	return false;
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

int GameDataManager::GetZombieSurvivalRound(ZombieType zombieType) const
{
	auto it = mZombieInfo.find(zombieType);
	if (it != mZombieInfo.end())
		return it->second.survivalRound;
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
	LOG_DEBUG("GameData") << "========== GameDataManager 数据（数值来自 gamedata.json） ==========";
	LOG_DEBUG("GameData") << "植物总数: " << mPlantInfo.size();
	for (const auto& pair : mPlantInfo) {
		const PlantInfo& info = pair.second;
		LOG_DEBUG("GameData") << "植物: " << info.animName << " (" << info.enumName << ")";
		LOG_DEBUG("GameData") << "  阳光: " << info.SunCost << "  冷却: " << info.Cooldown
			<< "s  缩放: " << info.scale;
		LOG_DEBUG("GameData") << "  纹理键: " << info.textureKey;
		LOG_DEBUG("GameData") << "  动画类型: " << static_cast<int>(info.animType);
		LOG_DEBUG("GameData") << "  偏移量: (" << info.offset.x << ", " << info.offset.y << ")";
	}

	LOG_DEBUG("GameData") << "僵尸总数: " << mZombieInfo.size();
	for (const auto& pair : mZombieInfo) {
		const ZombieInfo& info = pair.second;
		LOG_DEBUG("GameData") << "僵尸: " << info.animName << " (" << info.enumName << ")";
		LOG_DEBUG("GameData") << "  权重: " << info.weight << "  出现波: " << info.appearWave
			<< "  生存轮: " << info.survivalRound << "  缩放: " << info.scale;
		LOG_DEBUG("GameData") << "  动画类型: " << static_cast<int>(info.animType);
		LOG_DEBUG("GameData") << "  偏移量: (" << info.offset.x << ", " << info.offset.y << ")";
	}
	LOG_DEBUG("GameData") << "=============================================";
}

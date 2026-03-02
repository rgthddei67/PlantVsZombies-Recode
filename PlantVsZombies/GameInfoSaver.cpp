#include "GameInfoSaver.h"
#include "GameApp.h"
#include "./Game/Board.h"
#include "./Game/GameScene.h"
#include "./Game/AudioSystem.h"
#include "./Game/Bullet/Bullet.h"
#include "./Game/Plant/Plant.h"
#include "./Game/Zombie/Zombie.h"
#include "./Game/GameProgress.h"
#include "./Game/Sun.h"
#include "./Game/Bullet/BulletType.h"
#include "./Game/CardSlotManager.h"
#include "./Game/Card.h"
#include "./Game/CardComponent.h"
#include "./Game/TransformComponent.h"
#include "./Game/GameObjectManager.h"

bool GameInfoSaver::SavePlayerInfo()
{
	auto& gameApp = GameAPP::GetInstance();

	FileManager::CreateDirectory("./save");

	nlohmann::json j;
	j["difficulty"] = gameApp.Difficulty;
	j["adventureLevel"] = gameApp.mAdventureLevel;
	j["showPlantHP"] = gameApp.mShowPlantHP;
	j["showZombieHP"] = gameApp.mShowZombieHP;
	j["autoCollected"] = gameApp.mAutoCollected;
	j["soundVolume"] = AudioSystem::GetSoundVolume();
	j["musicVolume"] = AudioSystem::GetMusicVolume();

	j["havecards"] = gameApp.mHaveCards;

	return FileManager::SaveJsonFile("./save/PlayerInfo.json", j);
}

bool GameInfoSaver::LoadPlayerInfo()
{
	nlohmann::json j;
	if (!FileManager::LoadJsonFile("./save/PlayerInfo.json", j))
		return false;

	auto& gameApp = GameAPP::GetInstance();
	gameApp.Difficulty = j.value("difficulty", 1);
	gameApp.mAdventureLevel = j.value("adventureLevel", 1);
	gameApp.mShowPlantHP = j.value("showPlantHP", false);
	gameApp.mShowZombieHP = j.value("showZombieHP", false);
	gameApp.mAutoCollected = j.value("autoCollected", true);
	gameApp.mHaveCards = j.value("havecards",
		std::vector<PlantType>{PlantType::PLANT_PEASHOOTER});

	AudioSystem::SetSoundVolume(j.value("soundVolume", 0.5f));
	AudioSystem::SetMusicVolume(j.value("musicVolume", 0.5f));

	return true;
}

bool GameInfoSaver::SaveLevelData(Board* board, CardSlotManager* manager)
{
	if (board->mBoardState != BoardState::GAME) return false;
	FileManager::CreateDirectory("./save");

	nlohmann::json j;

	// Board 状态
	j["boardState"] = static_cast<int>(board->mBoardState);
	j["sun"] = board->mSun;
	j["currentWave"] = board->mCurrentWave;
	j["maxWave"] = board->mMaxWave;
	j["zombieCountDown"] = board->mZombieCountDown;
	j["totalZombieHP"] = board->mTotalZombieHP;
	j["currentWaveZombieHP"] = board->mCurrectWaveZombieHP;
	j["nextWaveSpawnZombieHP"] = board->mNextWaveSpawnZombieHP;
	j["zombieNumber"] = board->mZombieNumber;

	// 保存 EntityManager 的 ID 计数器
	j["nextPlantID"] = board->mEntityManager.GetNextPlantID();
	j["nextZombieID"] = board->mEntityManager.GetNextZombieID();
	j["nextBulletID"] = board->mEntityManager.GetNextBulletID();
	j["nextCoinID"] = board->mEntityManager.GetNextCoinID();

	// 植物
	nlohmann::json plantsArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllPlantIDs()) {
		auto plant = board->mEntityManager.GetPlant(id);
		if (!plant) continue;
		nlohmann::json p;
		p["id"] = id;
		p["type"] = static_cast<int>(plant->mPlantType);
		p["row"] = plant->mRow;
		p["column"] = plant->mColumn;
		p["health"] = plant->mPlantHealth;
		p["maxHealth"] = plant->mPlantMaxHealth;
		p["isSleeping"] = plant->mIsSleeping;
		p["animTrack"] = plant->GetCurrentTrackName();
		p["animFrame"] = plant->GetCurrentFrame();
		nlohmann::json extraData;
		plant->SaveExtraData(extraData);
		if (!extraData.empty()) {
			p["extraData"] = extraData;
		}
		plantsArr.push_back(p);
	}
	j["plants"] = plantsArr;

	// 僵尸
	nlohmann::json zombiesArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllZombieIDs()) {
		auto zombie = board->mEntityManager.GetZombie(id);
		if (!zombie) continue;
		nlohmann::json z;
		z["id"] = id;
		z["type"] = static_cast<int>(zombie->mZombieType);
		z["row"] = zombie->mRow;
		z["x"] = zombie->GetPosition().x;

		z["bodyHealth"] = zombie->mBodyHealth;
		z["bodyMaxHealth"] = zombie->mBodyMaxHealth;
		z["helmType"] = zombie->mHelmType;
		z["helmHealth"] = zombie->mHelmHealth;
		z["helmMaxHealth"] = zombie->mHelmMaxHealth;
		z["shieldType"] = zombie->mShieldType;
		z["shieldHealth"] = zombie->mShieldHealth;
		z["shieldMaxHealth"] = zombie->mShieldMaxHealth;

		z["spawnWave"] = zombie->mSpawnWave;
		z["attackDamage"] = zombie->mAttackDamage;
		z["needDropArm"] = zombie->mNeedDropArm;
		z["needDropHead"] = zombie->mNeedDropHead;
		zombie->SaveProtectedData(z);
		z["animTrack"] = zombie->GetCurrentTrackName();
		z["animFrame"] = zombie->GetCurrentFrame();
		nlohmann::json extraData;
		zombie->SaveExtraData(extraData);
		if (!extraData.empty()) {
			z["extraData"] = extraData;
		}
		zombiesArr.push_back(z);
	}
	j["zombies"] = zombiesArr;

	// 子弹
	nlohmann::json bulletsArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllBulletIDs()) {
		auto bullet = board->mEntityManager.GetBullet(id);
		if (!bullet) continue;
		nlohmann::json b;
		b["type"] = static_cast<int>(bullet->mBulletType);
		b["id"] = id;
		b["row"] = bullet->mRow;
		b["x"] = bullet->GetPosition().x;
		b["y"] = bullet->GetPosition().y;
		b["damage"] = bullet->GetBulletDamage();
		b["velocityX"] = bullet->GetVelocityX();
		b["velocityY"] = bullet->GetVelocityY();
		bulletsArr.push_back(b);
	}
	j["bullets"] = bulletsArr;

	// 太阳（Sun 继承 Coin，SunPoint 固定 25，只需保存位置）
	nlohmann::json sunsArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllCoinIDs()) {
		auto coin = board->mEntityManager.GetCoin(id);
		if (!coin) continue;
		auto sun = std::dynamic_pointer_cast<Sun>(coin);
		if (!sun) continue;
		nlohmann::json s;
		s["id"] = id;
		s["x"] = sun->GetPosition().x;
		s["y"] = sun->GetPosition().y;
		s["animTrack"] = sun->GetCurrentTrackName();
		s["animFrame"] = sun->GetCurrentFrame();
		sunsArr.push_back(s);
	}
	j["suns"] = sunsArr;

	// 卡牌
	nlohmann::json cardsArr = nlohmann::json::array();
	if (manager) {
		for (auto& weakCard : manager->GetCards()) {
			auto card = weakCard.lock();
			if (!card) continue;
			auto comp = card->GetComponent<CardComponent>();
			if (!comp) continue;
			auto transform = card->GetComponent<TransformComponent>();
			if (!transform) continue;
			nlohmann::json c;
			c["plantType"] = static_cast<int>(comp->GetPlantType());
			c["posX"] = transform->GetPosition().x;
			c["posY"] = transform->GetPosition().y;
			c["sunCost"] = comp->GetSunCost();
			c["cooldownTime"] = comp->GetCooldownTime();
			c["isCooldown"] = comp->IsCooldown();
			c["cooldownTimer"] = comp->GetCooldownTimer();
			cardsArr.push_back(c);
		}
	}
	j["cards"] = cardsArr;

	std::string filename = "./save/level" + std::to_string(board->mLevel) + "_data.json";
	return FileManager::SaveJsonFile(filename, j);
}

bool GameInfoSaver::LoadLevelData(Board* board, CardSlotManager* manager)
{
	std::string filename = "./save/level" + std::to_string(board->mLevel) + "_data.json";
	nlohmann::json j;
	if (!FileManager::LoadJsonFile(filename, j))
		return false;

	// 恢复 Board 状态
	board->mBoardState = static_cast<BoardState>(j.value("boardState", static_cast<int>(BoardState::GAME)));
	board->mSun = j.value("sun", 50);
	board->mCurrentWave = j.value("currentWave", 0);
	board->mMaxWave = j.value("maxWave", 10);
	board->mZombieCountDown = j.value("zombieCountDown", 20.0f);
	board->mTotalZombieHP = j.value("totalZombieHP", 0.0);
	board->mCurrectWaveZombieHP = j.value("currentWaveZombieHP", 0.0);
	board->mNextWaveSpawnZombieHP = j.value("nextWaveSpawnZombieHP", 0.0);
	board->mZombieNumber = j.value("zombieNumber", 0);

	// 恢复 EntityManager 的 ID 计数器（向后兼容：旧存档没有则使用默认值）
	board->mEntityManager.SetNextPlantID(j.value("nextPlantID", 1));
	board->mEntityManager.SetNextZombieID(j.value("nextZombieID", 1));
	board->mEntityManager.SetNextBulletID(j.value("nextBulletID", 1));
	board->mEntityManager.SetNextCoinID(j.value("nextCoinID", 1));

	// 恢复进度条
	if (board->mCurrentWave > 0) {
		auto gameProgress = board->mGameScene->GetGameProgress();
		gameProgress->SetActive(true);
		auto& res = ResourceManager::GetInstance();
		gameProgress->SetupFlags(res.GetTexture(ResourceKeys::Textures::IMAGE_FLAGMETER_PART_STICK)
			, res.GetTexture(ResourceKeys::Textures::IMAGE_FLAGMETER_PART_FLAG));
	}

	// 恢复植物
	for (auto& p : j.value("plants", nlohmann::json::array())) {
		PlantType type = static_cast<PlantType>(p["type"].get<int>());
		int row = p["row"].get<int>();
		int col = p["column"].get<int>();
		int health = p["health"].get<int>();
		int maxHealth = p["maxHealth"].get<int>();
		bool isSleeping = p["isSleeping"].get<bool>();
		int id = p.value("id", NULL_PLANT_ID);

		std::shared_ptr<Plant> plant;
		if (id != NULL_PLANT_ID) {
			plant = board->CreatePlantWithID(type, row, col, id);
		}
		else {
			plant = board->CreatePlant(type, row, col);
		}

		if (plant) {
			plant->mPlantHealth = health;
			plant->mPlantMaxHealth = maxHealth;
			plant->mIsSleeping = isSleeping;
			std::string track = p.value("animTrack", "");
			if (!track.empty()) {
				plant->PlayTrack(track);
				plant->SetCurrentFrame(p.value("animFrame", 0.0f));
			}
			if (p.contains("extraData")) {
				plant->LoadExtraData(p["extraData"]);
			}
		}
	}

	// 恢复僵尸
	for (auto& z : j.value("zombies", nlohmann::json::array())) {
		ZombieType type = static_cast<ZombieType>(z["type"].get<int>());
		int   row = z["row"].get<int>();
		float x = z["x"].get<float>();
		int   id = z.value("id", NULL_ZOMBIE_ID);

		std::shared_ptr<Zombie> zombie;
		if (id != NULL_ZOMBIE_ID) {
			zombie = board->CreateZombieWithID(type, row, x, 0.0f, id);
		}
		else {
			zombie = board->CreateZombie(type, row, x, 0.0f);
		}

		if (zombie) {
			zombie->mBodyHealth = z.value("bodyHealth", 270);
			zombie->mBodyMaxHealth = z.value("bodyMaxHealth", 270);
			zombie->mHelmType = static_cast<HelmType>(z.value("helmType",HelmType::HELMTYPE_NONE));
			zombie->mHelmHealth = z.value("helmHealth", 0);
			zombie->mHelmMaxHealth = z.value("helmMaxHealth", 0);
			zombie->mShieldType = static_cast<ShieldType>(z.value("shieldType", ShieldType::SHIELDTYPE_NONE));
			zombie->mShieldHealth = z.value("shieldHealth", 0);
			zombie->mShieldMaxHealth = z.value("shieldMaxHealth", 0);
			zombie->mSpawnWave = z.value("spawnWave", -1);
			zombie->mAttackDamage = z.value("attackDamage", 50);
			zombie->mNeedDropArm = z.value("needDropArm", true);
			zombie->mNeedDropHead = z.value("needDropHead", true);

			zombie->LoadProtectedData(z);
			std::string track = z.value("animTrack", "");
			if (!track.empty()) {
				zombie->PlayTrack(track);
				zombie->SetCurrentFrame(z.value("animFrame", 0.0f));
			}

			if (z.contains("extraData")) {
				zombie->LoadExtraData(z["extraData"]);
			}

			zombie->ZombieItemUpdate();
		}
	}

	// 恢复子弹
	for (auto& b : j.value("bullets", nlohmann::json::array())) {
		BulletType type = static_cast<BulletType>(b["type"].get<int>());
		int   row = b["row"].get<int>();
		float x = b["x"].get<float>();
		float y = b["y"].get<float>();
		int   id = b.value("id", NULL_BULLET_ID);

		std::shared_ptr<Bullet> bullet;
		if (id != NULL_BULLET_ID) {
			bullet = board->CreateBulletWithID(type, row, Vector(x, y), id);
		}
		else {
			bullet = board->CreateBullet(type, row, Vector(x, y));
		}

		if (bullet) {
			bullet->SetBulletDamage(b["damage"].get<int>());
			bullet->SetVelocityX(b["velocityX"].get<float>());
			bullet->SetVelocityY(b["velocityY"].get<float>());
		}
	}

	// 恢复太阳
	for (auto& s : j.value("suns", nlohmann::json::array())) {
		float x = s["x"].get<float>();
		float y = s["y"].get<float>();
		int  id = s.value("id", NULL_COIN_ID);

		std::shared_ptr<Sun> sun;
		if (id != NULL_COIN_ID) {
			sun = board->CreateSunWithID(Vector(x, y), false, id);
		}
		else {
			sun = board->CreateSun(Vector(x, y), false);
		}

		if (sun) {
			std::string track = s.value("animTrack", "");
			if (!track.empty()) {
				sun->PlayTrack(track);
				sun->SetCurrentFrame(s.value("animFrame", 0.0f));
			}
		}
	}

	// 恢复卡牌
	if (manager) {
		for (auto& c : j.value("cards", nlohmann::json::array())) {
			PlantType plantType = static_cast<PlantType>(c["plantType"].get<int>());
			float posX = c["posX"].get<float>();
			float posY = c["posY"].get<float>();
			int sunCost = c["sunCost"].get<int>();
			float cooldownTime = c["cooldownTime"].get<float>();
			bool isCooldown = c["isCooldown"].get<bool>();
			float cooldownTimer = c["cooldownTimer"].get<float>();

			auto card = GameObjectManager::GetInstance().CreateGameObjectImmediate<Card>(
				LAYER_UI, plantType, sunCost, cooldownTime, false);
			if (!card) continue;

			if (auto transform = card->GetComponent<TransformComponent>()) {
				transform->SetPosition(Vector(posX, posY));
			}
			if (isCooldown) {
				if (auto comp = card->GetComponent<CardComponent>()) {
					comp->RestoreCooldown(cooldownTimer, cooldownTime);
				}
			}
			manager->AddCard(card);
		}
	}

	// 恢复旗子升起状态
	if (board->mGameScene) {
		if (auto progress = board->mGameScene->GetGameProgress()) {
			progress->InitializeRaisedFlags(-10.0f);
		}
	}

	return true;
}

bool GameInfoSaver::DeleteLevelData(Board* board)
{
	std::string filename = "./save/level" + std::to_string(board->mLevel) + "_data.json";
	return FileManager::DeleteFile(filename);
}

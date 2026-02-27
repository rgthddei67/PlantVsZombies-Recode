#include "Board.h"
#include "Sun.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Plant/SunFlower.h"
#include "./Plant/Shooter.h"
#include "./Zombie/Zombie.h"
#include "./Zombie/ConeZombie.h"
#include "./Bullet/PeaBullet.h"
#include "./Plant/PeaShooter.h"
#include "./SceneManager.h"
#include "EntityManager.h"
#include "RenderOrder.h"
#include "GameScene.h"
#include "./Plant/GameDataManager.h"
#include "./GameProgress.h"
#include "../GameApp.h"
#include "../FileManager.h"
#include <unordered_set>
#include <climits>

void Board::InitializeCell(int rows, int cols)
{
	mRows = rows + 1;
	mColumns = cols + 1;
	mCells.resize(mRows);
	for (int i = 0; i < mRows; i++)
	{
		mCells[i].resize(mColumns);
		for (int j = 0; j < mColumns; j++)
		{
			Vector position(CELL_INITALIZE_POS_X + j * CELL_COLLIDER_SIZE_X,
				CELL_INITALIZE_POS_Y + i * CELL_COLLIDER_SIZE_Y);
			auto cell = GameObjectManager::GetInstance().CreateGameObject<Cell>(
				LAYER_BACKGROUND, i, j, position);
			mCells[i][j] = cell;
		}
	}
}

/*
void Board::DrawCell(SDL_Renderer* renderer)
{
	SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // 半透明绿色
	for (auto& row : mCells) {
		for (auto& cell : row) {
			Vector pos = cell->GetWorldPosition();
			SDL_FRect rect = { pos.x, pos.y, CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y };
			SDL_RenderDrawRectF(renderer, &rect);
		}
	}
}

void Board::Draw(SDL_Renderer* renderer)
{
	// DrawCell(renderer);
}
*/
std::shared_ptr<Sun> Board::CreateSun(const Vector& position, bool needAnimation)
{
	auto sun = GameObjectManager::GetInstance().CreateGameObject<Sun>
		(LAYER_GAME_COIN, this, position, 0.85f, "Sun",
			needAnimation, true);
	if (sun) {
		mEntityManager.AddCoin(sun);
	}

	return sun;
}

std::shared_ptr<Sun> Board::CreateSun(float x, float y, bool needAnimation) {
	return CreateSun(Vector(x, y), needAnimation);
}

std::shared_ptr<Plant> Board::CreatePlant(PlantType plantType, int row, int column, bool isPreview)
{
	// 检查行列是否有效
	if (row < 0 || row >= mRows || column < 0 || column >= mColumns) {
		std::cout << "无效的行列位置: (" << row << ", " << column << ")" << std::endl;
		return nullptr;
	}

	// 根据植物类型创建对应的植物
	// TODO: 新增植物也要改这里
	std::shared_ptr<Plant> plant = nullptr;

	switch (plantType) {
	case PlantType::PLANT_SUNFLOWER:
		plant = GameObjectManager::GetInstance().CreateGameObjectImmediate<SunFlower>(
			LAYER_GAME_PLANT,        // int renderOrder
			this,                    // Board* board
			PlantType::PLANT_SUNFLOWER,  // PlantType plantType
			row,                    // int row
			column,                 // int column
			AnimationType::ANIM_SUNFLOWER,  // AnimationType animType
			1.0f,                   // scale
			isPreview               // mIsPreview
		);
		break;

	case PlantType::PLANT_PEASHOOTER:
		plant = GameObjectManager::GetInstance().CreateGameObjectImmediate<PeaShooter>(
			LAYER_GAME_PLANT,
			this,
			PlantType::PLANT_PEASHOOTER,
			row,
			column,
			AnimationType::ANIM_PEASHOOTER,
			1.0f,
			isPreview
		);
		break;

	case PlantType::PLANT_WALLNUT:
		break;

	case PlantType::PLANT_CHERRYBOMB:
		break;

	case PlantType::PLANT_SNOWPEA:
		break;

	case PlantType::PLANT_CHOMPER:
		break;

	case PlantType::PLANT_REPEATER:
		break;

	case PlantType::PLANT_POTATOMINE:
		break;

	default:
		std::cout << "未知的植物类型: " << static_cast<int>(plantType) << std::endl;
		break;
	}

	if (plant && !isPreview) {
		mEntityManager.AddPlant(plant);

		// 将植物与格子关联
		auto cell = GetCell(row, column);
		if (cell)
		{
			cell->SetPlantID(plant->mPlantID);
		}
	}

	return plant;
}

std::shared_ptr<Zombie> Board::CreateZombie(ZombieType zombieType, int row, float x, float y, bool isPreview) {
	std::shared_ptr<Zombie> zombie = nullptr;

	if (row >= 0)
	{
		if (this->mBackGround == 0)
		{
			y = static_cast<float>(140 + row * 100);
		}
	}

	// TODO: 新增僵尸也要改这里
	switch (zombieType) {
	case ZombieType::ZOMBIE_NORMAL:
		zombie = GameObjectManager::GetInstance().CreateGameObjectImmediate<Zombie>(
			LAYER_GAME_ZOMBIE,
			this,
			ZombieType::ZOMBIE_NORMAL,
			x,
			y,
			row,
			AnimationType::ANIM_NORMAL_ZOMBIE,
			1.0f,
			isPreview);
		break;
	case ZombieType::ZOMBIE_TRAFFIC_CONE:
		zombie = GameObjectManager::GetInstance().CreateGameObjectImmediate<ConeZombie>(
			LAYER_GAME_ZOMBIE,
			this,
			ZombieType::ZOMBIE_TRAFFIC_CONE,
			x,
			y,
			row,
			AnimationType::ANIM_CONE_ZOMBIE,
			1.0f,
			isPreview);
		break;
	default:
		std::cout << "[Board::CreateZombie] 未知的僵尸类型" << std::endl;
		return nullptr;
	}

	if (zombie && !isPreview) {
		mEntityManager.AddZombie(zombie);

		zombie->mSpawnWave = this->mCurrentWave;
	}
	return zombie;
}

std::shared_ptr<Bullet> Board::CreateBullet(BulletType bulletType, int row, const Vector& position)
{
	std::shared_ptr<Bullet> bullet = nullptr;
	// TODO: 新增子弹也要改这里
	switch (bulletType) {
	case BulletType::BULLET_PEA:
		bullet = GameObjectManager::GetInstance().CreateGameObjectImmediate<PeaBullet>(
			LAYER_GAME_BULLET,
			this,
			BulletType::BULLET_PEA,
			row,
			ResourceManager::GetInstance().GetTexture(
				ResourceKeys::Textures::IMAGE_PROJECTILEPEA),
			Vector(10, 10),
			position);
		break;
	default:
		std::cout << "Board::CreateBullet未知的子弹类型" << std::endl;
		return nullptr;
	}

	if (bullet)
	{
		mEntityManager.AddBullet(bullet);
	}
	return bullet;
}

void Board::CleanupExpiredObjects()
{
	// 清理已过期的植物ID映射
	// TODO 如果其他地方也有存储植物ID,也要删除
	std::vector<int> removedPlants = mEntityManager.CleanupExpired();

	// 遍历被清理的植物ID，清除对应Cell中的植物ID
	for (int plantID : removedPlants) {
		CleanPlantFromCells(plantID);
	}
}

void Board::CleanPlantFromCells(int plantID)
{
	for (size_t i = 0; i < mCells.size(); i++) {
		for (size_t j = 0; j < mCells[i].size(); j++) {
			if (mCells[i][j]->GetPlantID() == plantID) {
				mCells[i][j]->ClearPlantID();
			}
		}
	}
}

void Board::UpdateSunFalling(float deltaTime)
{
	mSunCountDown -= deltaTime;
	if (mSunCountDown <= 0.0f)
	{
		mSunCountDown = SPAWN_SUN_TIME;
		Vector sunPos(
			GameRandom::Range(50.0f, 770.0f),      // 50~770
			GameRandom::Range(-110.0f, -20.0f)    // -110~-20
		);
		auto sun = CreateSun(sunPos, false);
		sun->StartLinearFall();
	}
}

void Board::UpdateLevel()
{
	if (mBoardState != BoardState::GAME) return;
	float deltaTime = DeltaTime::GetDeltaTime();
	UpdateSunFalling(deltaTime);
	UpdateZombieHP();

	if (mCurrentWave >= mMaxWave)
	{
		return;
	}

	mZombieCountDown -= deltaTime;

	if (mCurrentWave > 0 && mCurrectWaveZombieHP <= mNextWaveSpawnZombieHP)
	{
		mZombieCountDown = 0.0f;
	}

	if (mZombieCountDown <= 0.0f)
	{
		// 一大波僵尸处理
		if (mCurrentWave == 9)
		{
			mHugeWaveCountDown += deltaTime;
			if (!mHasHugeWaveSound)
			{
				mHasHugeWaveSound = true;
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HUGEWAVE, 0.7f);
				if (mGameScene)
					mGameScene->ShowPrompt(
						ResourceKeys::Textures::IMAGE_HUGE_WAVE_APPROACHING,
						0.4f,
						4.0f,
						0.3f);
			}
			if (mHugeWaveCountDown >= 7.5f)
			{
				mHugeWaveCountDown = 0.0f;
				mHasHugeWaveSound = false;
			}
			else
			{
				return;
			}
		}
		mZombieCountDown = NEXTWAVE_COUNT_MAX;
		mCurrentWave++;
		if (mCurrentWave == 1)
		{
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FIRSTWAVE, 0.7f);
			if (mGameScene) {
				auto gameProgress = mGameScene->GetGameProgress();
				gameProgress->SetActive(true);
				auto& res = ResourceManager::GetInstance();
				gameProgress->SetupFlags(res.GetTexture(ResourceKeys::Textures::IMAGE_FLAGMETER_PART_STICK)
					, res.GetTexture(ResourceKeys::Textures::IMAGE_FLAGMETER_PART_FLAG)
				);
			}
		}
		if (mCurrentWave == mMaxWave)
		{
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FINALWAVE, 0.7f);
			if (mGameScene)
				mGameScene->ShowPrompt(
					ResourceKeys::Textures::IMAGE_FINAL_WAVE,
					0.3f,
					2.0f,
					0.4f);
		}
		if (mCurrentWave % 10 == 0)
		{
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_AFTERHUGEWAVE, 0.7f);
		}

		TrySummonZombie();
		UpdateZombieHP();

		mNextWaveSpawnZombieHP = static_cast<double>
			(GameRandom::Range(0.5f, 0.65f) * mCurrectWaveZombieHP);
	}
}

void Board::CreatePreviewZombies()
{
	if (mBoardState != BoardState::CHOOSE_CARD || !mPreviewZombieList.empty()
		|| mSpawnZombieList.empty()) return;

	mPreviewZombieList.clear();
	for (ZombieType zombieType : mSpawnZombieList)
	{
		Vector spawnPosition = Vector(GameRandom::Range
		(mSpawnZombiePos1.x, mSpawnZombiePos2.x), GameRandom::Range
		(mSpawnZombiePos1.y, mSpawnZombiePos2.y));
		auto preview = this->
			CreateZombie(zombieType, -1, spawnPosition.x, spawnPosition.y, true);
		mPreviewZombieList.push_back(preview);
	}
}

void Board::DestroyPreviewZombies()
{
	if (mPreviewZombieList.empty()) return;

	for (auto& zombieWeak : mPreviewZombieList)
	{
		if (auto zombie = zombieWeak.lock())
		{
			zombie->Die();
		}
	}
	mPreviewZombieList.clear();
}

void Board::InitializeRows()
{
	mRowInfos.clear();
	mRowInfos.resize(mRows);
	for (int i = 0; i < mRows; i++)
	{
		mRowInfos[i].rowIndex = i;
		mRowInfos[i].weight = 1.0f;
		mRowInfos[i].smoothWeight = 1.0f;
		mRowInfos[i].loseMower = -3;
		mRowInfos[i].lastPicked = 0;
		mRowInfos[i].secondLastPicked = 0;
	}
}

void Board::SetRowLoseMower(int row)
{
	if (row < 0 || row >= static_cast<int>(mRowInfos.size())) return;
	mRowInfos[row].loseMower = mCurrentWave;
}

int Board::SelectSpawnRow()
{
	if (mRowInfos.empty()) InitializeRows();

	// 第一步：根据 loseMower 计算基础权重
	float totalWeight = 0.0f;
	for (int i = 0; i < mRows; i++)
	{
		int mowerTest = mCurrentWave - mRowInfos[i].loseMower;
		if (mowerTest <= 1)       mRowInfos[i].weight = 0.01f;
		else if (mowerTest <= 2)  mRowInfos[i].weight = 0.5f;
		else                      mRowInfos[i].weight = 1.0f;
		totalWeight += mRowInfos[i].weight;
	}

	// 第二步：计算平滑权重（避免重复选同一行）
	float smoothTotal = 0.0f;
	for (int i = 0; i < mRows; i++)
	{
		float wp = (totalWeight > 0.0f) ? (mRowInfos[i].weight / totalWeight) : 0.0f;
		if (wp >= ROW_WEIGHT_THRESHOLD)
		{
			float pLast = (6.0f * static_cast<float>(mRowInfos[i].lastPicked) * wp
				+ 6.0f * wp - 3.0f) / 4.0f;
			float pSecond = (static_cast<float>(mRowInfos[i].secondLastPicked) * wp
				+ wp - 1.0f) / 4.0f;
			float combined = pLast + pSecond;
			if (combined < 0.01f) combined = 0.01f;
			if (combined > 100.0f) combined = 100.0f;
			mRowInfos[i].smoothWeight = wp * combined;
		}
		else
		{
			mRowInfos[i].smoothWeight = 0.01f;
		}
		smoothTotal += mRowInfos[i].smoothWeight;
	}

	// 第三步：加权随机选行
	if (smoothTotal <= 0.0f) return mRows - 1;

	float randNum = GameRandom::Range(0.0f, smoothTotal);
	float cumulative = 0.0f;
	for (int i = 0; i < mRows - 1; i++)
	{
		cumulative += mRowInfos[i].smoothWeight;
		if (cumulative >= randNum) return i;
	}
	return mRows - 1;
}

ZombieType Board::GetWeightedRandomZombie()
{
	if (mSpawnZombieList.empty()) return ZombieType::ZOMBIE_NORMAL;

	int totalWeight = 0;
	for (ZombieType type : mSpawnZombieList)
		totalWeight += GameDataManager::GetInstance().GetZombieWeight(type);

	if (totalWeight <= 0) return mSpawnZombieList[0];

	int randVal = GameRandom::Range(0, totalWeight - 1);
	for (ZombieType type : mSpawnZombieList)
	{
		randVal -= GameDataManager::GetInstance().GetZombieWeight(type);
		if (randVal < 0) return type;
	}
	return mSpawnZombieList[0];
}

ZombieType Board::GetCheapestZombie()
{
	ZombieType cheapest = ZombieType::ZOMBIE_NORMAL;
	int minCost = INT_MAX;
	for (ZombieType type : mSpawnZombieList)
	{
		int cost = GameDataManager::GetInstance().GetZombieWeight(type);
		if (cost < minCost) { minCost = cost; cheapest = type; }
	}
	return cheapest;
}

ZombieType Board::PickZombieType(int remainingPoints)
{
	for (int attempt = 0; attempt < 1000; attempt++)
	{
		ZombieType type = GetWeightedRandomZombie();
		int cost = GameDataManager::GetInstance().GetZombieWeight(type);
		int minWave = GameDataManager::GetInstance().GetZombieAppearWave(type);
		if (remainingPoints >= cost && mCurrentWave >= minWave)
			return type;
	}
	return GetCheapestZombie();
}

void Board::TrySummonZombie()
{
	if (mCurrentWave > mMaxWave) return;

	int remainingPoints = CalculateWaveZombiePoints();
	int zombiesSpawned = 0;
	float x = static_cast<float>(SCENE_WIDTH) + 30;

	while (remainingPoints > 0 && zombiesSpawned < MAX_ZOMBIES_PER_WAVE)
	{
		ZombieType selected = PickZombieType(remainingPoints);
		int cost = GameDataManager::GetInstance().GetZombieWeight(selected);
		if (cost <= 0) break;

		int row = SelectSpawnRow();

		// 更新行追踪计数器
		for (int i = 0; i < mRows; i++)
		{
			if (mRowInfos[i].weight > 0.0f)
			{
				mRowInfos[i].lastPicked++;
				mRowInfos[i].secondLastPicked++;
			}
		}
		mRowInfos[row].secondLastPicked = mRowInfos[row].lastPicked;
		mRowInfos[row].lastPicked = 0;

		auto zombie = CreateZombie(selected, row, x, 0.0f, false);
		if (zombie)
		{
			zombiesSpawned++;
			remainingPoints -= cost;
		}
	}
}

int Board::CalculateWaveZombiePoints() const
{
	// 基础点数
	float points = (static_cast<float>(mCurrentWave) / 3 + 1.0f) * 1000.0f;

	points *= (GameAPP::GetInstance().Difficulty * 0.5f);

	// 判断是否为旗帜波
	bool isFlagWave = (mCurrentWave % 10 == 0);
	if (isFlagWave)
	{
		points *= 2.5f;
	}

	return static_cast<int>(points);
}

void Board::UpdateZombieHP()
{
	double TotalHP = 0, CurrectWaveHP = 0;
	int zombieNumber = 0;
	for (auto zombieID : mEntityManager.GetAllZombieIDs())
	{
		if (auto zombie = mEntityManager.GetZombie(zombieID))
		{
			zombieNumber++;
			if (zombie->IsMindControlled()) continue;	// 判断是不是魅惑

			double zombieHp = static_cast<double>(zombie->mBodyHealth +
				zombie->mHelmHealth + zombie->mShieldHealth);

			TotalHP += zombieHp;
			if (zombie->mSpawnWave == this->mCurrentWave)
			{
				CurrectWaveHP += zombieHp;
			}
		}
	}

	mTotalZombieHP = TotalHP;
	mCurrectWaveZombieHP = CurrectWaveHP;
	mZombieNumber = zombieNumber;
}

void Board::Update()
{
	CleanupExpiredObjects();
	UpdateLevel();
}

void Board::StartGame()
{
	DestroyPreviewZombies();
	mBoardState = BoardState::GAME;
	AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_DAY, -1);
}

void Board::GameOver()
{
	mBoardState = BoardState::LOSE_GAME;
	DeltaTime::SetPaused(true);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LOSTGAME, 0.6f);
	AudioSystem::StopMusic();
	if (mGameScene)
		mGameScene->GameOver();
}

void Board::LoadSpawnListFromJson()
{
	nlohmann::json data;
	if (!FileManager::LoadJsonFile("./resources/spawnlists.json", data)) return;
	if (!data.is_array()) return;

	for (auto& entry : data)
	{
		if (!entry.contains("level") || !entry.contains("zombies")) continue;
		if (entry["level"].get<int>() != mLevel) continue;

		mSpawnZombieList.clear();
		std::unordered_set<int> seen;
		for (auto& v : entry["zombies"])
		{
			int val = v.get<int>();
			if (val < 0 || val >= static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES)) continue;
			if (seen.count(val)) continue;
			seen.insert(val);
			mSpawnZombieList.push_back(static_cast<ZombieType>(val));
		}
		return;
	}
	// 没找到对应关卡配置，保持默认 ZOMBIE_NORMAL（不清空）
}
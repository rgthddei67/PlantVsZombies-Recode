#include "Board.h"
#include "Sun.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Plant/SunFlower.h"
#include "./Plant/Shooter.h"
#include "./Zombie/Zombie.h"
#include "./Bullet/PeaBullet.h"
#include "./Plant/PeaShooter.h"
#include "EntityManager.h"
#include "RenderOrder.h"
#include "./Plant/GameDataManager.h"
#include "../GameApp.h"

void Board::InitializeCell(int rows, int cols)
{
	StartGame();	// TODO 记得删

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

std::shared_ptr<Sun> Board::CreateSun(const float& x, const float& y, bool needAnimation) {
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

std::shared_ptr<Zombie> Board::CreateZombie(ZombieType zombieType, int row, const float& x, bool isPreview) {
	std::shared_ptr<Zombie> zombie = nullptr;

	// TODO: 新增僵尸也要改这里
	switch (zombieType) {
	case ZombieType::ZOMBIE_NORMAL:
		zombie = GameObjectManager::GetInstance().CreateGameObjectImmediate<Zombie>(
			LAYER_GAME_ZOMBIE,
			this,
			ZombieType::ZOMBIE_NORMAL,
			x,
			row,
			AnimationType::ANIM_NORMAL_ZOMBIE,
			1.0f,
			isPreview);
		break;
	default:
		std::cout << "未知的僵尸类型" << std::endl;
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

float Board::RowToY(int row)
{
	return 0;
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
	// if (mBoardState != BoardState::GAME) return;
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
		}
		if (mCurrentWave == mMaxWave)
		{
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FINALWAVE, 0.7f);
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

void Board::TrySummonZombie()
{
	if (mCurrentWave > mMaxWave) return;

	// 计算本波总点数
	int totalPoints = CalculateWaveZombiePoints();
	int remainingPoints = totalPoints;
	int zombiesSpawned = 0;

	// 过滤出当前波可用的僵尸类型
	std::vector<ZombieType> availableTypes;
	for (ZombieType type : mSpawnZombieList)
	{
		if (mCurrentWave >= GameDataManager::GetInstance().GetZombieAppearWave(type))
		{
			availableTypes.push_back(type);
		}
	}

	// 如果没有可用类型，至少保证普通僵尸可用（防止空列表）
	if (availableTypes.empty())
	{
		availableTypes.push_back(ZombieType::ZOMBIE_NORMAL);
	}

	// 循环生成僵尸，直到点数不足或达到波次上限
	while (remainingPoints > 0 && zombiesSpawned < NORMALMODE_MAX_WAVE_ZOMBIE)
	{
		// 从可用类型中随机选择一种
		int index = GameRandom::Range(0, static_cast<int>(availableTypes.size()) - 1);
		ZombieType type = availableTypes[index];
		int weight = GameDataManager::GetInstance().GetZombieWeight(type);
		if (weight <= 0) continue;

		// 允许最后一只略微超点 仍然生成这只僵尸（超一点也无妨）
		/*
		if (remainingPoints < weight && remainingPoints > 0)
		{

		}
		*/

		int row = GameRandom::Range(0, mRows - 1);

		float x = 840.0f;

		auto zombie = CreateZombie(type, row, x, false);
		if (zombie)
		{
			zombiesSpawned++;
			remainingPoints -= weight;
		}
		else
		{
			// 创建失败
			continue;
		}
	}
}

int Board::CalculateWaveZombiePoints() const
{
	// 基础点数
	int points = mCurrentWave / 2 + 1;

	points *= GameAPP::GetInstance().Difficulty;

	// 判断是否为旗帜波
	bool isFlagWave = (mCurrentWave % 10 == 0);
	if (isFlagWave)
	{
		points = static_cast<int>(points * 3.5f);
	}

	return points;
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
	mBoardState = BoardState::GAME;
	AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_DAY, -1);
}
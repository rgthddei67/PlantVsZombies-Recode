#include "Board.h"
#include "Sun.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Plant/SunFlower.h"
#include "./Plant/Shooter.h"
#include "./Zombie/Zombie.h"
#include "./Bullet/PeaBullet.h"
#include "./Plant/PeaShooter.h"
#include "./SceneManager.h"
#include "EntityManager.h"
#include "RenderOrder.h"
#include "GameScene.h"
#include "./Plant/GameDataManager.h"
#include "./GameProgress.h"
#include "../GameApp.h"

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

void Board::TrySummonZombie()
{
	if (mCurrentWave > mMaxWave) return;  // 超出最大波次，不再生成

	// 计算本波总点数
	int totalPoints = CalculateWaveZombiePoints();
	int remainingPoints = totalPoints;
	int zombiesSpawned = 0;

	// 获取可用僵尸类型（根据 appearWave 过滤）
	std::vector<ZombieType> availableTypes;
	for (ZombieType type : mSpawnZombieList)
	{
		if (GameDataManager::GetInstance().GetZombieAppearWave(type) <= mCurrentWave)
		{
			availableTypes.push_back(type);
		}
	}

	// 如果没有可用类型，保底使用普通僵尸
	if (availableTypes.empty())
	{
		availableTypes.push_back(ZombieType::ZOMBIE_NORMAL);
	}

	// 计算权重指数因子 alpha，范围 [-1, 1]
	float alpha = -1.0f;
	if (mMaxWave > 1)
	{
		alpha = 2.0f * (mCurrentWave - 1) / (mMaxWave - 1) - 1.0f;
	}

	// 循环生成僵尸，直到点数不足或达到波次上限
	while (remainingPoints > 0 && zombiesSpawned < MAX_ZOMBIES_PER_WAVE)
	{
		// 计算每个可用类型的得分（权重^alpha）
		std::vector<float> scores;
		scores.reserve(availableTypes.size());
		for (ZombieType type : availableTypes)
		{
			int weight = GameDataManager::GetInstance().GetZombieWeight(type);
			// 避免权重为0导致的数学异常（正常情况下权重>0）
			float score = (weight > 0) ? std::pow(static_cast<float>(weight), alpha) : 0.0f;
			scores.push_back(score);
		}

		// 加权随机选择一个类型
		const ZombieType& selected = GameRandom::WeightedChoice(availableTypes, scores);

		int weight = GameDataManager::GetInstance().GetZombieWeight(selected);
		if (weight <= 0) continue; // 安全保护

		// 随机选择行（0 到 mRows-1）
		int row = GameRandom::Range(0, mRows - 1);

		float x = static_cast<float>(SCENE_WIDTH) + 30;

		auto zombie = CreateZombie(selected, row, x, 0, false);
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
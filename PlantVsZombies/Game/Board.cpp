#include "Board.h"
#include "../Logger.h"
#include "LawnMower.h"
#include "Shovel.h"
#include "Sun.h"
#include "Trophy.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Zombie/Zombie.h"

#include "EntityManager.h"
#include "RenderOrder.h"
#include "GameScene.h"
#include "./Plant/GameDataManager.h"
#include "./GameProgress.h"
#include "../GameApp.h"
#include "../FileManager.h"
#include <unordered_set>
#include <climits>
#include <algorithm>   // std::max, std::swap
#include <cmath>       // std::lround

// 复刻原版 TodCommon.TodAnimateCurve(..., TodCurves.Linear)：把 round 在 [startRound,endRound]
// 归一化并钳到 [0,1]，在 [fromVal,toVal] 间线性插值，四舍五入取整。
static int SurvivalCurveLerp(int startRound, int endRound, int round,
                             int fromVal, int toVal)
{
	if (endRound <= startRound) return toVal;            // 防 0 除
	float t = static_cast<float>(round - startRound)
	        / static_cast<float>(endRound - startRound);
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	return static_cast<int>(std::lround(
		static_cast<float>(fromVal) + t * static_cast<float>(toVal - fromVal)));
}

Board::Board(GameScene* gameScene, Background background, int level)
{
	mGameScene = gameScene;
	mLevel = level;
	mBackGround = background;
	mIsSurvival = (level == SURVIVAL_ENDLESS_LEVEL || level == SURVIVAL_ENDLESS_NIGHT_LEVEL);

	if (mLevel >= 1)
	{
		mLevelName.clear();
		int mBigLevel = (mLevel - 1) / 9 + 1;
		int mSmallLevel = (mLevel - 1) % 9 + 1;
		mLevelName = u8"关卡 " + std::to_string(mBigLevel) + u8"-" + std::to_string(mSmallLevel);
	}
	mSpawnZombieList.reserve(32);
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);
	mPreviewZombieList.reserve(32);

	if (mIsSurvival)
	{
		mSurvivalRound = 1;
		mPerkManager.Clear();   // 新生存局：词条清零（读档时由 Load 覆盖）
		mMaxWave = SURVIVAL_WAVES_PER_ROUND;
		BuildSurvivalSpawnList(mSurvivalRound);
		UpdateSurvivalLevelName();
	}
	else if (mLevel > 0)
	{
		LoadSpawnListFromJson();
	}

	CreatePreviewZombies();
	InitializeCell();
	InitializeRows();
}

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
			Cell* cell = GameObjectManager::GetInstance().CreateGameObject<Cell>(
				LAYER_BACKGROUND, i, j, position);
			mCells[i][j] = cell;
		}
	}
}

void Board::CreateBoom(const Vector& position, int damage)
{
	damage = mPerkManager.ScalePlantDamage(damage);   // 词条：全体植物伤害（含瞬时伤害；入口缩放使秒杀阈值与扣血一致）
	g_particleSystem->EmitEffect("CherryBomb", position);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CHERRYBOMB, 0.4f);
	std::vector<int> zombieIDs = mEntityManager.GetAllZombieIDs();
	for (auto zombieID : zombieIDs)
	{
		if (auto zombie = mEntityManager.GetZombie(zombieID)) {
			Vector zombiePositon = zombie->GetPosition();
			if (std::abs(zombiePositon.x - position.x) <= 130.0f &&
				std::abs(zombiePositon.y - position.y) <= 130.0f)
			{
				if (zombie->mBodyHealth <= damage)
				{
					zombie->Charred();
				}
				else
				{
					zombie->TakeDamage(damage);
				}
			}
		}
	}
}

Sun* Board::CreateSun(const Vector& position, bool needAnimation)
{
	auto sun = GameObjectManager::GetInstance().CreateGameObjectAsShared<Sun>
		(LAYER_GAME_COIN, this, position, 0.85f, "Sun",
			needAnimation, true);
	if (sun) {
		mEntityManager.AddCoin(sun);
	}

	return sun.get();
}

Sun* Board::CreateSun(float x, float y, bool needAnimation) {
	return CreateSun(Vector(x, y), needAnimation);
}

SmallSun* Board::CreateSmallSun(const Vector& position, bool needAnimation)
{
	auto sun = GameObjectManager::GetInstance().CreateGameObjectAsShared<SmallSun>
		(LAYER_GAME_COIN, this, position, 0.6f, "SmallSun",
			needAnimation, true);
	if (sun) {
		mEntityManager.AddCoin(sun);
	}

	return sun.get();
}

SmallSun* Board::CreateSmallSun(float x, float y, bool needAnimation) {
	return CreateSmallSun(Vector(x, y), needAnimation);
}

void Board::CreateTrophy(const Vector& position)
{
	if (mTrophySpawned) return;
	mTrophySpawned = true;
	auto trophy = GameObjectManager::GetInstance().CreateGameObjectAsShared<Trophy>(
		LAYER_GAME_COIN, this, position);
	if (trophy) {
		mEntityManager.AddCoin(trophy);
	}
}

Plant* Board::CreatePlant(PlantType plantType, int row, int column, bool skipsettings, bool isPreview)
{
	// 检查行列是否有效
	if (row < 0 || row >= mRows || column < 0 || column >= mColumns) {
		LOG_ERROR("Board") << "无效的行列位置: (" << row << ", " << column << ")";
		return nullptr;
	}

	// 根据植物类型创建对应的植物
	std::shared_ptr<Plant> plant = GameAPP::GetInstance().InstantiatePlant(plantType, this, row, column, isPreview);

	if (plant && !isPreview && !skipsettings) {
		mEntityManager.AddPlant(plant);

		// 将植物与格子关联
		Cell* cell = GetCell(row, column);
		if (cell)
		{
			cell->SetPlantID(plant->mPlantID);
		}
	}

	return plant.get();
}

Zombie* Board::CreateZombie(ZombieType zombieType, int row, float x, bool skipsettings, bool isPreview) {
	// y 始终由 row 决定，调用方无法自定义。GetZombieSpawnY 对非法行返回 -1，兜底为 0。
	float y = GetZombieSpawnY(row);
	if (y < 0.0f) y = 0.0f;

	std::shared_ptr<Zombie> zombie = GameAPP::GetInstance().InstantiateZombie
	(zombieType, this, x, y, row, isPreview);
	if (!zombie) return nullptr;

	mZombieNumber++;

	if (!isPreview && !skipsettings) {
		mEntityManager.AddZombie(zombie);
		zombie->mSpawnWave = this->mCurrentWave;
		// 按当前难度来源对整只僵尸血量施加全局倍率（默认 1，目前由生存模式按轮次提供）。
		// 仅在此波次生成路径施加；读档走 CreateZombieWithID 直接还原已含倍率的存档血量，不重复缩放。
		zombie->ApplyHealthMultiplier(GetZombieHpMultiplier());
		// 词条②：按当前词条层数设定出生免伤次数（无词条→0）。读档走 CreateZombieWithID 不在此赋值，
		// 由 LoadProtectedData 还原（与血量倍率同契约）。
		zombie->mFreeHitsRemaining = GetPerkManager().GetZombieInvulnHits();
	}
	return zombie.get();
}

Bullet* Board::CreateBullet(BulletType bulletType, int row, const Vector& position, bool skipsettings)
{
	// 使用对象池创建子弹
	BulletPool* bulletPool = GameObjectManager::GetInstance().GetBulletPool();
	if (!bulletPool) {
		LOG_ERROR("Board") << "CreateBullet 对象池未初始化";
		return nullptr;
	}

	// 从对象池获取子弹（shared_ptr 局部变量，用于把 weak_ptr 注册进 EntityManager）
	std::shared_ptr<Bullet> bullet = bulletPool->AcquireShared
	(this, bulletType, row, Vector(10, 10), position);

	if (bullet && !skipsettings) {
		mEntityManager.AddBullet(bullet);
	}

	return bullet.get();
}

inline void Board::CleanupExpiredObjects()
{
	// 清理已过期的植物ID映射
	// TODO 如果其他地方也有存储植物ID,也要删除
	std::vector<int> removedPlants = mEntityManager.CleanupExpired();

	// 遍历被清理的植物ID，清除对应Cell中的植物ID
	for (int plantID : removedPlants) {
		CleanPlantFromCells(plantID);
	}
}

inline void Board::CleanPlantFromCells(int plantID)
{
	for (size_t i = 0; i < mCells.size(); i++) {
		for (size_t j = 0; j < mCells[i].size(); j++) {
			if (mCells[i][j]->GetPlantID() == plantID) {
				mCells[i][j]->ClearPlantID();
			}
		}
	}
}

inline void Board::UpdateSunFalling(float deltaTime)
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

	if (mBackGround == Background::GROUND_DAY || mBackGround == Background::WATER_POOL ||
		mBackGround == Background::ROOF) {
		UpdateSunFalling(deltaTime);
	}

	mUpdateHPCheckTimer += deltaTime;
	if (mUpdateHPCheckTimer >= 0.5f)
	{
		mUpdateHPCheckTimer = 0.0f;
		UpdateZombieHP();
	}

	// 词条③：植物回血全局脉冲（生存专用；无词条→GetPlantRegenPerPulse()=0，整循环跳过，零开销）。
	// O(n) 遍历只在脉冲触发的那一帧发生（每 5s 一次），非每帧扫描。
	mPlantRegenTimer += deltaTime;
	if (mPlantRegenTimer >= mPerkManager.GetPlantRegenInterval())
	{
		mPlantRegenTimer = 0.0f;
		int heal = mPerkManager.GetPlantRegenPerPulse();
		if (heal > 0)
		{
			for (int id : mEntityManager.GetAllPlantIDs())
			{
				Plant* p = mEntityManager.GetPlant(id);
				if (!p || p->IsPreview()) continue;
				int cap = mPerkManager.GetPlantRegenHpCap(p->mPlantMaxHealth);
				if (p->mPlantHealth < cap)
				{
					int healed = p->mPlantHealth + heal;
					p->mPlantHealth = (healed > cap) ? cap : healed;
				}
			}
		}
	}

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
		if ((mCurrentWave + 1) % 10 == 0)
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

		mNextWaveSpawnZombieHP = static_cast<long long>
			(GameRandom::Range(0.5f, 0.65f) * static_cast<double>(mCurrectWaveZombieHP));
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
		// 预览僵尸需要在生成区内随机散落（任意 y），不绑定网格行，故走自由摆放路径。
		auto preview = GameAPP::GetInstance().InstantiateZombieFree(
			zombieType, this, spawnPosition.x, spawnPosition.y);
		if (!preview) continue;
		// 与 Zombie::Die 中的 mZombieNumber-- 保持平衡（预览僵尸 board == this，销毁时会递减）。
		mZombieNumber++;
		mPreviewZombieList.push_back(preview.get());
	}
}

void Board::DestroyPreviewZombies()
{
	if (mPreviewZombieList.empty()) return;

	for (Zombie* zombie : mPreviewZombieList)
	{
		if (zombie)
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

inline int Board::SelectSpawnRow()
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

int Board::GetSurvivalPickWeight(ZombieType type) const
{
	int base = GameDataManager::GetInstance().GetZombieWeight(type);
	if (!mIsSurvival) return base;
	int flags = mSurvivalRound - 1;   // 已完成轮数(对应原版 survivalFlagsCompleted)
	if (type == ZombieType::ZOMBIE_NORMAL)
		return SurvivalCurveLerp(SURVIVAL_DILUTE_START_FLAG, SURVIVAL_DILUTE_END_FLAG,
		                         flags, base, base / 10);   // 原版 Normal: base → base/10
	if (type == ZombieType::ZOMBIE_TRAFFIC_CONE)
		return SurvivalCurveLerp(SURVIVAL_DILUTE_START_FLAG, SURVIVAL_DILUTE_END_FLAG,
		                         flags, base, base / 4);     // 原版 Cone: base → base/4
	return base;
}

inline ZombieType Board::GetWeightedRandomZombie()
{
	if (mSpawnZombieList.empty()) return ZombieType::ZOMBIE_NORMAL;

	int totalWeight = 0;
	for (ZombieType type : mSpawnZombieList)
		totalWeight += GetSurvivalPickWeight(type);

	if (totalWeight <= 0) return mSpawnZombieList[0];

	int randVal = GameRandom::Range(0, totalWeight - 1);
	for (ZombieType type : mSpawnZombieList)
	{
		randVal -= GetSurvivalPickWeight(type);
		if (randVal < 0) return type;
	}
	return mSpawnZombieList[0];
}

inline ZombieType Board::GetCheapestZombie()
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

inline ZombieType Board::PickZombieType(int remainingPoints)
{
	for (int attempt = 0; attempt < 1000; attempt++)
	{
		ZombieType type = GetWeightedRandomZombie();
		int cost = GameDataManager::GetInstance().GetZombieWeight(type);
		int minWave = GameDataManager::GetInstance().GetZombieAppearWave(type);
		if (remainingPoints >= cost && (mIsSurvival || mCurrentWave >= minWave))
			return type;
	}
	return GetCheapestZombie();
}

inline void Board::TrySummonZombie()
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

		auto zombie = CreateZombie(selected, row, x);
		if (zombie)
		{
			zombiesSpawned++;
			remainingPoints -= cost;
		}
	}
}

inline int Board::CalculateWaveZombiePoints() const
{
	// 基础点数
	float points = (static_cast<float>(mCurrentWave) / 3 + 1.0f) * 1000.0f;

	points *= (GameAPP::GetInstance().Difficulty * 0.5f);

	// 生存模式：单波点数预算随轮次递增（每轮 mCurrentWave 会重置，故由轮次系数补偿）
	if (mIsSurvival)
	{
		points *= (1.0f + SURVIVAL_BUDGET_GROWTH * static_cast<float>(mSurvivalRound - 1));
	}

	// 判断是否为旗帜波
	bool isFlagWave = (mCurrentWave % 10 == 0);
	if (isFlagWave)
	{
		points *= 2.5f;
	}

	// 防溢出：float 超过 INT_MAX 时 static_cast<int> 是 UB(实测得 INT_MIN)，
	// 会让本波 remainingPoints<=0 而一只僵尸都不刷。钳到 INT_MAX。
	// 注意 (float)INT_MAX == 2147483648.0f(2^31,比 INT_MAX 大 1)，故用 >= 比较。
	if (points >= static_cast<float>(INT_MAX))
	{
		return INT_MAX;
	}
	return static_cast<int>(points);
}

inline void Board::UpdateZombieHP()
{
	long long TotalHP = 0, CurrectWaveHP = 0;
	for (auto zombieID : mEntityManager.GetAllZombieIDs())
	{
		if (auto zombie = mEntityManager.GetZombie(zombieID))
		{
			if (zombie->IsMindControlled()) continue;	// 判断是不是魅惑

			long long zombieHp = static_cast<long long>(zombie->mBodyHealth) +
				zombie->mHelmHealth + zombie->mShieldHealth;

			TotalHP += zombieHp;
			if (zombie->mSpawnWave == this->mCurrentWave)
			{
				CurrectWaveHP += zombieHp;
			}
		}
	}

	mTotalZombieHP = TotalHP;
	mCurrectWaveZombieHP = CurrectWaveHP;
}

void Board::Update()
{
	CleanupExpiredObjects();
	UpdateLevel();
}

void Board::StartGame()
{
	DestroyPreviewZombies();
	if (mGameScene) {
		mGameScene->ShowShovel();
	}
	if (!mIsLoadSave) {
		InitializeMowers();
	}
	mBoardState = BoardState::GAME;
	if (mBackGround == Background::GROUND_DAY) {
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_DAY, -1);
	}
	else if (mBackGround == Background::GROUND_NIGHT) {
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_NIGHT, -1);
	}
	else {
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_DAY, -1);
	}
}

void Board::GameOver()
{
	DeltaTime::SetPaused(true);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LOSTGAME, 0.65f);
	AudioSystem::StopMusic();
	if (mGameScene)
		mGameScene->GameOver();
	mBoardState = BoardState::LOSE_GAME;
}

void Board::OnSurvivalRoundClear()
{
	if (!mIsSurvival) return;

	// 推进轮次并重置本轮波次状态（轮次单列在 mSurvivalRound）
	mSurvivalRound++;
	mCurrentWave = 0;
	mMaxWave = SURVIVAL_WAVES_PER_ROUND;
	mZombieCountDown = 10.0f;
	mTrophySpawned = false;
	mHasHugeWaveSound = false;
	mHugeWaveCountDown = 0.0f;
	mNextWaveSpawnZombieHP = 0;
	mCurrectWaveZombieHP = 0;
	mTotalZombieHP = 0;

	// 重算难度（解锁更强僵尸）+ 刷新关卡名
	BuildSurvivalSpawnList(mSurvivalRound);
	UpdateSurvivalLevelName();

	// 回到选卡：暂停波次推进
	mBoardState = BoardState::CHOOSE_CARD;

	// 通知场景：先弹词条选择（选卡之前）；玩家选定/跳过后由 ApplyPerkSelection 链式进选卡。
	if (mGameScene)
		mGameScene->BeginSurvivalPerkSelect();
}

void Board::BuildSurvivalSpawnList(int round)
{
	// 仅使用本作已实现的可生成僵尸类型（0~5）
	mSpawnZombieList.clear();
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);                 // 普通
	if (round >= 2) mSpawnZombieList.push_back(ZombieType::ZOMBIE_TRAFFIC_CONE); // 路障
	if (round >= 3) {
		mSpawnZombieList.push_back(ZombieType::ZOMBIE_POLEVAULTER);       // 撑杆
		mSpawnZombieList.push_back(ZombieType::ZOMBIE_NEWSPAPER);         // 读报
	}
	if (round >= 4) mSpawnZombieList.push_back(ZombieType::ZOMBIE_BUCKET);       // 铁桶
	if (round >= 5) mSpawnZombieList.push_back(ZombieType::ZOMBIE_FASTBUCKET);   // 快速铁桶
	if (round >= 6) mSpawnZombieList.push_back(ZombieType::ZOMBIE_FASTPAPER);    // 加强读报
}

void Board::UpdateSurvivalLevelName()
{
	if (mLevel == 1000) {
		mLevelName = u8"生存模式：白天无尽 第" + std::to_string(mSurvivalRound) + u8"轮";
	}
	else if (mLevel == 1001) {
		mLevelName = u8"生存模式：黑夜无尽 第" + std::to_string(mSurvivalRound) + u8"轮";
	}
	else {
		mLevelName = u8"生存模式：未知无尽 第" + std::to_string(mSurvivalRound) + u8"轮";
	}
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
		if (entry.contains("waves"))
		{
			int waves = entry["waves"].get<int>();
			if (waves > 0)
				mMaxWave = waves;
		}
		return;
	}
	// 没找到对应关卡配置，保持默认 ZOMBIE_NORMAL（不清空）
}

Plant* Board::CreatePlantWithID(PlantType type, int row, int col, int id) {
	Cell* cell = GetCell(row, col);
	if (cell && cell->GetPlantID() != NULL_PLANT_ID) {
		return nullptr;
	}
	// 走 GameApp 工厂拿 shared_ptr 用于 EntityManager 注册
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) {
		LOG_ERROR("Board") << "无效的行列位置: (" << row << ", " << col << ")";
		return nullptr;
	}
	std::shared_ptr<Plant> plant = GameAPP::GetInstance().InstantiatePlant(type, this, row, col, false);
	if (plant) {
		mEntityManager.AddPlantWithID(plant, id);
		if (cell) {
			cell->SetPlantID(id);
		}
	}
	return plant.get();
}

Zombie* Board::CreateZombieWithID(ZombieType type, int row, float x, int id) {
	// y 始终由 row 决定（读档僵尸只持久化 row + x，y 不入存档）。
	float y = GetZombieSpawnY(row);
	if (y < 0.0f) y = 0.0f;

	std::shared_ptr<Zombie> zombie = GameAPP::GetInstance().InstantiateZombie
	(type, this, x, y, row, false);
	if (!zombie) return nullptr;
	mZombieNumber++;
	mEntityManager.AddZombieWithID(zombie, id);
	zombie->mSpawnWave = this->mCurrentWave;
	return zombie.get();
}

Bullet* Board::CreateBulletWithID(BulletType type, int row, const Vector& pos, int id) {
	BulletPool* bulletPool = GameObjectManager::GetInstance().GetBulletPool();
	if (!bulletPool) {
		LOG_ERROR("Board") << "CreateBulletWithID 对象池未初始化";
		return nullptr;
	}
	std::shared_ptr<Bullet> bullet = bulletPool->AcquireShared(this, type, row, Vector(10, 10), pos);
	if (bullet) {
		mEntityManager.AddBulletWithID(bullet, id);
	}
	return bullet.get();
}

Sun* Board::CreateSunWithID(const Vector& pos, bool fromSky, int id) {
	auto sun = GameObjectManager::GetInstance().CreateGameObjectAsShared<Sun>
		(LAYER_GAME_COIN, this, pos, 0.85f, "Sun",
			fromSky, true);
	if (sun) {
		mEntityManager.AddCoinWithID(sun, id);
	}
	return sun.get();
}

SmallSun* Board::CreateSmallSunWithID(const Vector& pos, bool fromSky, int id) {
	auto sun = GameObjectManager::GetInstance().CreateGameObjectAsShared<SmallSun>
		(LAYER_GAME_COIN, this, pos, 0.6f, "SmallSun",
			fromSky, true);
	if (sun) {
		mEntityManager.AddCoinWithID(sun, id);
	}
	return sun.get();
}

Trophy* Board::CreateTrophyWithID(const Vector& pos, int id) {
	if (mTrophySpawned) return nullptr;
	mTrophySpawned = true;
	auto trophy = GameObjectManager::GetInstance().CreateGameObjectAsShared<Trophy>(
		LAYER_GAME_COIN, this, pos);
	if (trophy) {
		mEntityManager.AddCoinWithID(trophy, id);
	}
	return trophy.get();
}

std::weak_ptr<Shovel> Board::CreateShovel() {
	if (!mShovel.expired())
		return mShovel;

	auto shovel = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Shovel>(LAYER_UI, this);
	mShovel = shovel;
	return mShovel;
}

void Board::ActivateShovel()
{
	if (auto shovel = mShovel.lock()) {
		mCursorObjectManager.Activate(CursorObjectType::SHOVEL, [shovel]() {
			shovel->ReturnHome();
			});
		shovel->Activate();
	}
}

Mower* Board::CreateMower(MowerType type, int row)
{
	float x = 160.0f;
	float y = 135.0f + row * 100.0f;

	auto mower = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Mower>(
		LAYER_GAME_OBJECT, this, type, AnimationType::ANIM_LAWNMOWER, x, y, row);

	if (mower) {
		mEntityManager.AddMower(mower);
	}
	return mower.get();
}

Mower* Board::CreateMowerWithID(MowerType type, int row, float x, float y, int id)
{
	auto mower = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Mower>(
		LAYER_GAME_OBJECT, this, type, AnimationType::ANIM_LAWNMOWER, x, y, row);

	if (mower) {
		mEntityManager.AddMowerWithID(mower, id);
	}
	return mower.get();
}

void Board::InitializeMowers()
{
	for (int row = 0; row < mRows; row++) {
		CreateMower(MowerType::LAWN, row);
	}
}

float Board::GetZombieSpawnY(int row) const {
	if (row < 0 || row >= mRows) {
		LOG_ERROR("Board") << "GetZombieSpawnY: 无效的行索引: " << row;
		return -1.0f;
	}

	if (this->mBackGround == Background::GROUND_DAY ||
		this->mBackGround == Background::GROUND_NIGHT)
	{
		return static_cast<float>(140 + row * 100);
	}
	else {
		return 0.0f;
	}
}
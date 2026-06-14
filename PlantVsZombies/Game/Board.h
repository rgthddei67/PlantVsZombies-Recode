#pragma once
#ifndef _BOARD_H
#define _BOARD_H
#include "Cell.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./Plant/PlantType.h"
#include "./Zombie/ZombieType.h"
#include "./Bullet/BulletType.h"
#include "EntityManager.h"
#include "CursorObjectManager.h"
#include <vector>
#include <memory>

class GameInfoSaver;
class GameScene;
class Sun;
class Coin;
class Plant;
class Zombie;
class Bullet;
class Trophy;
class Shovel;
class Mower;
enum class MowerType;

enum class Background {
	GROUND_DAY,
	GROUND_NIGHT,
	WATER_POOL,
	NIGHT_WATER_POOL,
	ROOF,
	NIGHT_ROOF
};

struct RowInfo {
	int rowIndex = 0;
	float weight = 1.0f;
	float smoothWeight = 1.0f;
	int loseMower = -3;   // 割草机丢失时的波次（-3 使第1波权重正常为1.0）
	int lastPicked = 0;    // 上次被选中后经过的僵尸数
	int secondLastPicked = 0;
};

constexpr int MAX_SUN = 9990;
constexpr float NEXTWAVE_COUNT_MAX = 25.0f;
constexpr float SPAWN_SUN_TIME = 15.0f;
constexpr int MAX_ZOMBIES_PER_WAVE = 120;	// 普通模式一波最大僵尸数量

// ===== 生存模式设置 =====
constexpr int   SURVIVAL_ENDLESS_LEVEL = 1000;   // 生存模式专用 level 号（> 50，避开冒险关推进逻辑）
constexpr int   SURVIVAL_WAVES_PER_ROUND = 10;   // 每轮（每面旗）波数，10 与"第10波=一大波"逻辑对齐
constexpr float SURVIVAL_BUDGET_GROWTH = 0.55f;  // 每轮单波点数预算增长系数
constexpr float SURVIVAL_HP_GROWTH     = 0.05f;  // 每轮僵尸全局血量倍率线性增长系数（可调）：mult = 1 + x*(轮次-1)

enum class BoardState {
	CHOOSE_CARD,
	GAME,
	LOSE_GAME,
	WIN,
	NONE,
};

class Board {
public:
	friend GameScene;
	friend GameInfoSaver;

	BoardState mBoardState = BoardState::CHOOSE_CARD;
	GameScene* mGameScene = nullptr;
	std::string mLevelName = "关卡 1-1";
	int mLevel = -1;	// 冒险模式当前关卡序号
	Background mBackGround = Background::GROUND_DAY; // 背景图
	int mRows = 5;	// 行数
	int mColumns = 8; // 列数
	std::weak_ptr<Shovel> mShovel;
	CursorObjectManager mCursorObjectManager;
	int mSun = 50;
	float mSunCountDown = 5.0f;
	EntityManager mEntityManager;
	int mCurrentWave = 0;			// 当前波
	int mMaxWave = 10;		// 关卡总波数
	float mZombieCountDown = 20.0f;		// 下一波僵尸倒计时
	long long mTotalZombieHP = 0;		// 在场全部僵尸血量
	long long mCurrectWaveZombieHP = 0;	// 本波僵尸血量
	long long mNextWaveSpawnZombieHP = 0;		// 下一波僵尸刷新血量

	int mZombieNumber = 0;
	bool mTrophySpawned = false;  // 防止重复生成

	bool mIsSurvival = false;     // 是否为生存模式（无尽）
	int  mSurvivalRound = 1;      // 当前第几面旗（轮次，从 1 起）

	Vector mSpawnZombiePos1 = Vector(1180, 85);			// 左上角坐标
	Vector mSpawnZombiePos2 = Vector(1500, 581);		// 右下角坐标

	std::vector<Zombie*> mPreviewZombieList;  // 预览僵尸（选卡阶段）；Board 显式管理生命周期，Die() 后立刻 clear()

	// 外层表示行（rows） 内层columns。Cell 所有权在 GameObjectManager；这里仅做格子寻址
	std::vector<std::vector<Cell*>> mCells;

private:
	std::vector<ZombieType> mSpawnZombieList;	// 本关出怪表
	float mHugeWaveCountDown = 0.0f;	// 一大波倒计时
	float mUpdateHPCheckTimer = 0.0f;	// 僵尸血量检查计时器
	bool mHasHugeWaveSound = false;		// 有无放过一大波音乐
	bool mIsLoadSave = false;	// 是否正在加载存档

	std::vector<RowInfo> mRowInfos;
	static constexpr float ROW_WEIGHT_THRESHOLD = 1e-6f;

	void LoadSpawnListFromJson();
	void InitializeRows();
	inline int SelectSpawnRow();
	inline ZombieType PickZombieType(int remainingPoints);
	inline ZombieType GetWeightedRandomZombie();
	inline ZombieType GetCheapestZombie();

	float GetZombieSpawnY(int row) const;

public:
	Board(GameScene* gameScene, Background background, int level);

	inline void AddSun(int amount)
	{
		int temp = mSun + amount;
		if (temp > MAX_SUN)
		{
			mSun = MAX_SUN;
			return;
		}
		mSun += amount;
	}

	inline void SubSun(int amount)
	{
		mSun -= amount;
	}

	inline int GetSun() { return mSun; }

	void SetZombieSpawnList(std::vector<ZombieType>& zombieTypeList) {
		this->mSpawnZombieList = zombieTypeList;
	}

	// 初始化格子 默认5行9列
	void InitializeCell(int rows = 4, int cols = 8);

	// 获取格子。返回原始指针：Cell 所有权在 GameObjectManager，调用方仅做非所有 view
	Cell* GetCell(int row, int col) {
		if (row >= 0 && row < mRows && col >= 0 && col < mColumns) {
			return mCells[row][col];
		}
		return nullptr;
	}

	// 创建僵尸：x 为任意像素横坐标，y 始终由 row 决定（不可自定义）。
	// 需要自由摆放（任意 y）的预览/UI 僵尸请改用 GameAPP::InstantiateZombieFree。
	Zombie* CreateZombie(ZombieType zombieType, int row, float x, bool skipsettings = false, bool isPreview = false);

	// 创建太阳
	Sun* CreateSun(const Vector& position, bool needAnimation = false);

	// 创建太阳
	Sun* CreateSun(float x, float y, bool needAnimation = false);

	// 创建奖杯
	void CreateTrophy(const Vector& position);

	// 创建植物
	Plant* CreatePlant(PlantType plantType, int row, int column, bool skipsettings = false, bool isPreview = false);

	// 创建铲子（保持 weak_ptr：mShovel 是跨帧成员，需要悬垂检测）
	std::weak_ptr<Shovel> CreateShovel();

	// 激活铲子
	void ActivateShovel();

	// 创建子弹
	Bullet* CreateBullet(BulletType plantType, int row, const Vector& position, bool skipsettings = false);

	// 创建樱桃爆炸效果
	void CreateBoom(const Vector& position, int damage = 1800);

	// 带指定 ID 创建实体（用于读档）
	Plant* CreatePlantWithID(PlantType type, int row, int col, int id);
	Zombie* CreateZombieWithID(ZombieType type, int row, float x, int id);
	Bullet* CreateBulletWithID(BulletType type, int row, const Vector& pos, int id);
	Sun* CreateSunWithID(const Vector& pos, bool fromSky, int id);
	Trophy* CreateTrophyWithID(const Vector& pos, int id);

	// 更新关卡
	void UpdateLevel();

	// 清理删除的对象
	inline void CleanupExpiredObjects();

	// 从所有Cell中清除指定植物ID
	inline void CleanPlantFromCells(int plantID);

	inline void UpdateSunFalling(float deltaTime);

	inline void UpdateZombieHP();

	// 尝试生成本波僵尸
	inline void TrySummonZombie();

	// 计算当前波的总点数
	inline int CalculateWaveZombiePoints() const;

	// 选好卡，开始游戏
	void StartGame();

	// 游戏结束
	void GameOver();

	// 生存模式：一轮（一面旗）清空后推进到下一轮并回到选卡
	void OnSurvivalRoundClear();

	// 生存模式：根据轮次重建出怪表
	void BuildSurvivalSpawnList(int round);

	// 生存模式：根据当前轮次刷新关卡名（"生存模式：无尽 第N面旗"）
	void UpdateSurvivalLevelName();

	// 僵尸全局血量倍率聚合点：默认 1，按当前生效的难度来源叠乘。
	// 目前唯一来源是生存模式（线性 1 + SURVIVAL_HP_GROWTH*(轮次-1)）；
	// 未来其他模式（困难冒险、悬赏关等）若需血量倍率，在此处继续叠乘即可，调用方无需改动。
	// 新波次僵尸生成时(CreateZombie)对其 body/头盔/护盾血量整体乘此系数；读档(CreateZombieWithID)不乘，避免二次叠加。
	double GetZombieHpMultiplier() const {
		double multiplier = 1.0;
		if (mIsSurvival)
			multiplier *= (1.0 + SURVIVAL_HP_GROWTH * static_cast<double>(mSurvivalRound - 1));
		return multiplier;
	}

	void Update();

	// 创建预览僵尸
	void CreatePreviewZombies();

	// 销毁所有预览僵尸
	void DestroyPreviewZombies();

	// 割草机触发时调用（预留接口）
	void SetRowLoseMower(int row);

	// 小推车
	Mower* CreateMower(MowerType type, int row);
	Mower* CreateMowerWithID(MowerType type, int row, float x, float y, int id);
	void InitializeMowers();
};
#endif
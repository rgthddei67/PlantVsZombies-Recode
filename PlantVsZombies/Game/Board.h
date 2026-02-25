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
#include <vector>
#include <memory>

class GameScene;
class Sun;
class Coin;
class Plant;
class Zombie;
class Bullet;

constexpr int MAX_SUN = 9990;
constexpr float NEXTWAVE_COUNT_MAX = 25.0f;
constexpr float SPAWN_SUN_TIME = 15.0f;
constexpr int MAX_ZOMBIES_PER_WAVE = 120;	// 普通模式一波最大僵尸数量

enum class BoardState {
	CHOOSE_CARD,
	GAME,
	LOSE_GAME,
	NONE,
};

class Board {
public:
	BoardState mBoardState = BoardState::CHOOSE_CARD;
	GameScene* mGameScene = nullptr;
	std::string mLevelName = "关卡 1-1";
	int mLevel = -1;	// 冒险模式当前关卡序号
	int mBackGround = 0; // 背景图
	int mRows = 5;	// 行数
	int mColumns = 8; // 列数
	int mSun = 50;
	float mSunCountDown = 5.0f;
	int mNextPlantID = 1;	// 下一个植物的ID
	int mNextCoinID = 1;	// 下一个Coin的ID
	EntityManager mEntityManager;
	int mCurrentWave = 0;			// 当前波
	int mMaxWave = 10;		// 关卡总波数
	float mZombieCountDown = 18.0f;		// 下一波僵尸倒计时
	double mTotalZombieHP = 0;		// 在场全部僵尸血量
	double mCurrectWaveZombieHP = 0;	// 本波僵尸血量
	double mNextWaveSpawnZombieHP = 0;		// 下一波僵尸刷新血量

	int mZombieNumber = 0;

	Vector mSpawnZombiePos1 = Vector(1180, 85);			// 左上角坐标
	Vector mSpawnZombiePos2 = Vector(1500, 581);		// 右下角坐标

	std::vector<std::weak_ptr<Zombie>> mPreviewZombieList;

	// 外层表示行（rows） 内层columns
	std::vector<std::vector<std::shared_ptr<Cell>>> mCells;

private:
	std::vector<ZombieType> mSpawnZombieList;	// 本关出怪表
	float mHugeWaveCountDown = 0.0f;	// 一大波倒计时
	bool mHasHugeWaveSound = false;		// 有无放过一大波音乐

public:
	Board(GameScene* gameScene, int level)
	{
		mGameScene = gameScene;
		mLevel = level;
		if (mLevel >= 1)
		{
			mLevelName.clear();
			int mBigLevel = mLevel / 10 + 1;
			int mSmallLevel = mLevel % 10;
			mLevelName = u8"关卡 " + std::to_string(mBigLevel) + u8"-" + std::to_string(mSmallLevel);
		}
		mSpawnZombieList.reserve(16);
		mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);
		mPreviewZombieList.reserve(16);
		// TODO: 写根据配置读取出怪
		CreatePreviewZombies();
		InitializeCell();
	}

	void AddSun(int amount) 
	{ 
		int temp = mSun + amount;
		if (temp > MAX_SUN)
		{
			mSun = MAX_SUN;
			return;
		}
		mSun += amount; 
	}

	void SubSun(int amount) 
	{ 
		mSun -= amount;
	}

	int GetSun() { return mSun; }

	void SetZombieSpawnList(std::vector<ZombieType>& zombieTypeList) {
		this->mSpawnZombieList = zombieTypeList;
	}

	// 初始化格子 默认5行9列
	void InitializeCell(int rows = 4, int cols = 8);

	// 获取格子智能指针
	std::shared_ptr<Cell> GetCell(int row, int col) {
		if (row >= 0 && row < mRows && col >= 0 && col < mColumns) {
			return mCells[row][col];
		}
		return nullptr;
	}

	// 创建僵尸 有row就用row，如果row<0，则使用y
	std::shared_ptr<Zombie> CreateZombie(ZombieType zombieType, int row, float x, float y, bool isPreview = false);

	// 创建太阳
	std::shared_ptr<Sun> CreateSun(const Vector& position, bool needAnimation = false);

	// 创建太阳
	std::shared_ptr<Sun> CreateSun(float x, float y, bool needAnimation = false);

	// 创建植物
	std::shared_ptr<Plant> CreatePlant(PlantType plantType, int row, int column, bool isPreview = false);

	// 创建子弹
	std::shared_ptr<Bullet> CreateBullet(BulletType plantType, int row, const Vector& position);

	// 更新关卡
	void UpdateLevel();

	// 渲染网格（调试用）
	//void DrawCell();

	//void Draw();

	// 清理删除的对象
	void CleanupExpiredObjects(); 

	// 从所有Cell中清除指定植物ID
	void CleanPlantFromCells(int plantID);

	void UpdateSunFalling(float deltaTime);

	void UpdateZombieHP();

	// 尝试生成本波僵尸
	void TrySummonZombie();

	// 计算当前波的总点数
	int CalculateWaveZombiePoints() const;

	// 选好卡，开始游戏
	void StartGame();

	// 游戏结束
	void GameOver();

	void Update();

	// 创建预览僵尸
	void CreatePreviewZombies();

	// 销毁所有预览僵尸
	void DestroyPreviewZombies();

};
#endif
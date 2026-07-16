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
#include "Perk/SurvivalPerkManager.h"
#include <vector>
#include <memory>

class GameInfoSaver;
class GameScene;
class Sun;
class SmallSun;
class Coin;
class Plant;
class Zombie;
class Bullet;
class Trophy;
class Crater;
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

namespace {
	constexpr int MAX_SUN = 9990;
	constexpr float NEXTWAVE_COUNT_MAX = 25.0f;
	constexpr float SPAWN_SUN_TIME = 15.0f;
	constexpr int MAX_ZOMBIES_PER_WAVE = 250;	// 一波最大僵尸数量

	// ===== 生存模式设置 =====
	constexpr int   SURVIVAL_ENDLESS_LEVEL = 1000;       // 白天无尽专用 level 号（> 50，避开冒险关推进逻辑）
	constexpr int   SURVIVAL_ENDLESS_NIGHT_LEVEL = 1001; // 黑夜无尽专用 level 号（背景走 GROUND_NIGHT，逻辑同 1000）
	constexpr int   SURVIVAL_WAVES_PER_ROUND = 10;   // 每轮（每面旗）波数，10 与"第10波=一大波"逻辑对齐
	constexpr float SURVIVAL_BUDGET_GROWTH = 0.55f;  // 每轮单波点数预算增长系数
	constexpr float SURVIVAL_HP_GROWTH = 0.05f;  // 每轮僵尸全局血量倍率线性增长系数（可调）：mult = 1 + x*(轮次-1)
	// 生存模式出怪池子组装(见 BuildSurvivalSpawnList)
	constexpr int SURVIVAL_RANDOM_POOL_START_ROUND = 3;   // 第几轮起改为"普通+随机子集"
	constexpr int SURVIVAL_POOL_BASE_EXTRA = 1;   // 第3轮的随机种类数(除普通外)
	constexpr int SURVIVAL_POOL_GROWTH_EVERY = 2;   // 每多少轮 +1 种(缓慢增长)
	// 旗数递减(复刻原版 TodAnimateCurve(18,50,flags,0,15))：深局提前解锁强僵尸；
	// 当前阵容(survivalRound 最高6、18旗才起步)下休眠，为未来高 survivalRound 僵尸预留。
	constexpr int SURVIVAL_UNLOCK_REDUCE_START_FLAG = 18;
	constexpr int SURVIVAL_UNLOCK_REDUCE_END_FLAG = 50;
	constexpr int SURVIVAL_UNLOCK_REDUCE_MAX = 15;
	// 杂兵稀释(复刻原版 Normal→base/10、Cone→base/4，TodAnimateCurve(10,50,...))：仅作用于抽中权重。
	constexpr int SURVIVAL_DILUTE_START_FLAG = 10;
	constexpr int SURVIVAL_DILUTE_END_FLAG = 50;
}

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
	int64_t mTotalZombieHP = 0;		// 在场全部僵尸血量
	int64_t mCurrectWaveZombieHP = 0;	// 本波僵尸血量
	int64_t mNextWaveSpawnZombieHP = 0;		// 下一波僵尸刷新血量

	int mZombieNumber = 0;

	// 全局节拍帧计数（随游戏时间推进：60Hz 基准，暂停冻结、倍速等比加速，入存档）。
	// 用途：舞王/伴舞全队共舞节拍源——所有舞者从同一计数推导动作，不互相通信也能整齐划一。
	int mBoardFrame = 0;
	float mBoardFrameAccum = 0.0f;	// 游戏时间→节拍帧的亚帧余量（不入存档，读档损失<1帧无感）

	// 舞蹈节拍帧 0~22 循环，每拍 12 逻辑步(0.2s)，等价原版 100Hz 的 20cs/拍、23 拍一循环。
	// 0~11 = 舞步段(anim_walk)，12~22 = 举手段(anim_armraise)；补充召唤只在节拍==12 触发。
	int GetDanceBeatFrame() const { return mBoardFrame % (12 * 23) / 12; }

	bool mTrophySpawned = false;  // 防止重复生成
	std::weak_ptr<Trophy> mTrophy;  // 每关至多一个；所有权在 GameObjectManager，此处仅供存档定位

	// 毁灭菇弹坑：所有权在 GameObjectManager（到时自毁），此处 weak_ptr 仅供寻址/存档
	std::vector<std::weak_ptr<Crater>> mCraters;

	bool mIsSurvival = false;     // 是否为生存模式（无尽）
	int  mSurvivalRound = 1;      // 当前第几面旗（轮次，从 1 起）
	SurvivalPerkManager mPerkManager;   // 生存模式词条（非生存关恒空）

	Vector mSpawnZombiePos1 = Vector(1180, 85);			// 左上角坐标
	Vector mSpawnZombiePos2 = Vector(1500, 581);		// 右下角坐标

	std::vector<Zombie*> mPreviewZombieList;  // 预览僵尸（选卡阶段）；Board 显式管理生命周期，Die() 后立刻 clear()

	// 外层表示行（rows） 内层columns。Cell 所有权在 GameObjectManager；这里仅做格子寻址
	std::vector<std::vector<Cell*>> mCells;

private:
	std::vector<ZombieType> mSpawnZombieList;	// 本关出怪表
	float mHugeWaveCountDown = 0.0f;	// 一大波倒计时
	float mUpdateHPCheckTimer = 0.0f;	// 僵尸血量检查计时器
	float mPlantRegenTimer = 0.0f;	// 词条③：全场植物回血脉冲计时器
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
	// 生存模式"抽中权重"：对 NORMAL/CONE 随轮稀释(仅供 GetWeightedRandomZombie；成本侧仍用 GetZombieWeight)
	inline int GetSurvivalPickWeight(ZombieType type) const;

public:
	// 该行僵尸的落脚 y（由地图行几何派生）。公开原因：伴舞出土裁剪要用“行地面线”而非
	// 僵尸自身坐标定裁剪底边——换新地图/行高时自动适配（主人指示）。
	float GetZombieSpawnY(int row) const;

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

	const std::vector<ZombieType>& GetSpawnZombieList() const { return mSpawnZombieList; }

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

	// 创建小阳光（阳光菇幼年产物：缩放 0.6 / 价值 15，行为同 Sun）
	SmallSun* CreateSmallSun(const Vector& position, bool needAnimation = false);
	SmallSun* CreateSmallSun(float x, float y, bool needAnimation = false);

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

	// 毁灭菇爆炸：半径 250 圆形判定、波及全部行、跳过魅惑僵尸；Charred 阈值逻辑同 CreateBoom
	void CreateDoomBoom(const Vector& position, int damage = 1800);

	// 添加/查询弹坑（毁灭菇）。AddCrater 由爆炸与读档共用；timeLeft 读档时传剩余值
	Crater* AddCrater(int row, int column, float timeLeft);
	bool HasCraterAt(int row, int column);   // 顺带惰性清理已消散的 weak_ptr

	// 带指定 ID 创建实体（用于读档）
	Plant* CreatePlantWithID(PlantType type, int row, int col, int id);
	Zombie* CreateZombieWithID(ZombieType type, int row, float x, int id);
	Bullet* CreateBulletWithID(BulletType type, int row, const Vector& pos, int id);
	Sun* CreateSunWithID(const Vector& pos, bool fromSky, int id);
	SmallSun* CreateSmallSunWithID(const Vector& pos, bool fromSky, int id);

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

	// 推进并生成下一波（Update 倒计时归零与开发者面板「下一波」共用入口）
	void SummonNextWave();

	// 选好卡，开始游戏
	void StartGame();

	// 游戏结束
	void GameOver();

	// 根据场景播放音乐
	void PlayBackgroundMusic();

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
	SurvivalPerkManager&       GetPerkManager()       { return mPerkManager; }
	const SurvivalPerkManager& GetPerkManager() const { return mPerkManager; }

	double GetZombieHpMultiplier() const {
		double multiplier = 1.0;
		if (mIsSurvival)
			multiplier *= (1.0 + SURVIVAL_HP_GROWTH * static_cast<double>(mSurvivalRound - 1));
		multiplier *= mPerkManager.GetZombieHealthMultiplier();   // 词条：僵尸血量（空时=1.0）
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
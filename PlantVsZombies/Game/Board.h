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
#include <string>

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

/** 黑夜天气强度；CLEAR 是所有倍率与视觉效果的单位元。 */
enum class RainIntensity {
	CLEAR,
	LIGHT,
	MEDIUM,
	HEAVY
};

/** 仅在大雨阶段存在的台风强度；NONE 表示没有附加台风。 */
enum class TyphoonStrength {
	NONE,
	TYPHOON,
	SEVERE,
	SUPER
};

/** 风实际吹向的棋盘方向；使用“吹向”口径，避免与气象学来向混淆。 */
enum class WindDirection {
	NONE,
	TOWARD_HOUSE,
	TOWARD_FRONT
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
	constexpr float SPAWN_SUN_TIME = 14.0f;       // 日间天降普通阳光的生成间隔，单位：游戏秒
	constexpr float POOL_SUN_SPAWN_TIME = 13.0f;  // 日间泳池水面小阳光的生成间隔，单位：游戏秒
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
	constexpr int SURVIVAL_POOL_MAX_TYPES = 8;   // 最终池子上限（包含必出的普通僵尸）
	constexpr int SURVIVAL_POOL_JITTER_MIN = 1;  // 基础种类数的随机波动幅度
	constexpr int SURVIVAL_POOL_JITTER_MAX = 2;
	// 旗数递减(复刻原版 TodAnimateCurve(18,50,flags,0,15))：深局提前解锁强僵尸；
	// 当前阵容(survivalRound 最高7、18旗才起步)下休眠，为未来高 survivalRound 僵尸预留。
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
	float mPoolSunCountDown = POOL_SUN_SPAWN_TIME;
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

	/** 返回最近一次僵尸指标采样得到的敌对、有头、未垂死僵尸数，供动态音乐与 AutoTest 使用。 */
	int GetHostileZombieCountForMusic() const { return mHostileZombieCountForMusic; }

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
	float mUpdateZombieMetricsTimer = 0.0f;	// 僵尸血量与音乐敌对数的合并采样计时器
	int mHostileZombieCountForMusic = 0;	// 每 0.5 游戏秒刷新，避免动态音乐每帧重复扫描全部僵尸
	float mPlantRegenTimer = 0.0f;	// 词条③：全场植物回血脉冲计时器
	bool mHasHugeWaveSound = false;		// 有无放过一大波音乐
	bool mHasHugeWaveMusicBurst = false;	// 本次一大波警告是否已强制加入鼓组
	bool mIsLoadSave = false;	// 是否正在加载存档

	// 黑夜随机天气。weatherTimer 在 CLEAR 时表示距下一场雨，在下雨时表示本场剩余时间。
	RainIntensity mRainIntensity = RainIntensity::CLEAR;
	RainIntensity mPreviousRainIntensity = RainIntensity::CLEAR; // 两秒平滑过渡开始前的雨势
	RainIntensity mForecastRainIntensity = RainIntensity::CLEAR; // 已发布预警对应的下一天气
	RainIntensity mActualForecastRainIntensity = RainIntensity::CLEAR; // 预警发布时锁定的真实下一天气
	float mWeatherTimer = 0.0f;
	float mWeatherTransitionTimer = 0.0f; // 当前天气过渡的剩余时间（秒，随游戏速度缩放）
	float mLightningTimer = 0.0f;
	float mRainSplashTimer = 0.0f;       // 距下一次地面水花的秒数；瞬态视觉无需写入存档
	bool mWeatherInitialized = false;   // 旧档缺天气字段时由 StartGame 首次初始化
	bool mRainCanIntensify = false;     // 仅初始小雨可增强；首次切档后永久转入衰减链
	bool mRainCanHold = false;          // 新雨首段为中/大雨时允许一次同档续期，避免无限维持
	bool mWeatherForecastReady = false; // true 表示公开预报与真实下一天气均已锁定、等待揭晓
	bool mPendingHeavyTyphoonPrepared = false; // 大雨预警期已锁定台风等级，切档时消费而不重 roll
	TyphoonStrength mPendingHeavyTyphoonStrength = TyphoonStrength::NONE; // 下一场大雨的待生效台风等级
	WindDirection mPendingHeavyWindDirection = WindDirection::NONE; // 待生效台风初始吹向；与等级一并锁定
	float mPendingHeavyTyphoonStrengthTimer = 0.0f; // 待生效台风首档维持时长（游戏秒）
	float mPendingHeavyWindDirectionTimer = 0.0f; // 待生效台风首次重抽风向倒计时（游戏秒）
	float mPendingHeavyWindGustTimer = 0.0f; // 待生效台风首次阵风倒计时（游戏秒）
	int mPendingHeavyTyphoonGustsRemaining = 0; // 待生效台风首档阵风预算
	int mPendingHeavyRainPromptVariant = 0; // 同等级三句古风警报中的锁定编号（0～2）
	bool mHeavyRainPromptShown = false; // 当前锁定预报是否已经弹出过提前 5 秒的大雨警报
	bool mRainVisualActive = false;     // 纯运行期标记，防读档/生存轮间重复发射同一场雨
	std::string mRainVisualEffectName;  // 当前雨丝特效名；风向切换时只停止旧雨而不清空其他粒子
	float mWindParticleTimer = 0.0f;    // 距下一批风线粒子的游戏秒数；瞬态视觉不入存档
	TyphoonStrength mTyphoonStrength = TyphoonStrength::NONE; // 大雨附加台风；离开大雨立即清空
	WindDirection mWindDirection = WindDirection::NONE;       // 当前风实际吹向，台风期间分段独立重抽
	float mTyphoonStrengthTimer = 0.0f; // 当前台风强度距下一档衰减的游戏秒数
	float mWindDirectionTimer = 0.0f;   // 距下一次风向独立重抽的游戏秒数
	float mWindGustTimer = 0.0f;        // 非阵风期间距下一次阵风开始的游戏秒数
	int mTyphoonGustsRemaining = 0;     // 本次台风阶段尚可触发的阵风次数
	bool mTyphoonGustActive = false;    // true 时锁定本次阵风强度/风向并连续吹动僵尸
	TyphoonStrength mActiveGustStrength = TyphoonStrength::NONE; // 阵风开始时锁定的强度，避免中途衰减突变
	WindDirection mActiveGustDirection = WindDirection::NONE;    // 阵风开始时锁定的吹向，避免中途翻转
	float mActiveGustDuration = 0.0f;   // 当前阵风完整持续时间（游戏秒）
	float mActiveGustTimer = 0.0f;      // 当前阵风剩余时间（游戏秒）
	float mActiveGustPlantMoveTimer = 0.0f; // 距本次阵风植物整格结算的游戏秒数
	bool mActiveGustPlantMoved = false; // 本次阵风是否已结算植物，防读档后重复移动
	int mWeakWeatherPhasesSinceHeavy = 0; // 后期连续非大雨新天气数；达到上限后下轮保底大雨并进入存档
	int mHeavyPhasesWithoutTyphoon = 0; // 连续未命中台风的新大雨阶段数；用于保底并进入存档
	int mEliteDancersSpawnedThisWave = 0; // 当前波已生成的精英舞王数量；用于每波上限并进入存档
	int mReinforcedDoorsSpawnedThisWave = 0; // 当前波正式生成的加固铁门数量；上限计数并进入存档
	int mElitePolevaultersSpawnedThisWave = 0; // 当前波正式生成的精英撑杆数量；上限计数并进入存档
	int mEliteScaredyShroomsPlanted = 0; // 本关累计种下的精英胆小菇数量；死亡或铲除不返还次数
	int mLastTyphoonMovedPlants = 0;    // 最近一次阵风移动的植物数，仅供观测和测试
	int mLastTyphoonLostPlants = 0;     // 最近一次阵风吹出棋盘或吹入弹坑的植物数，仅供观测和测试

	std::vector<RowInfo> mRowInfos;
	static constexpr float ROW_WEIGHT_THRESHOLD = 1e-6f;
	float mCellInitialY = CELL_INITALIZE_POS_Y;
	float mCellHeight = CELL_COLLIDER_SIZE_Y;

	// 屏幕抖动状态（见 ShakeBoard）。timer 递减到 0 即结束；重复触发直接覆盖重置
	float mShakeTimer = 0.0f;         // 剩余秒数，<=0 = 未抖动
	float mShakeDuration = 0.12f;     // 本次抖动总时长（秒）
	float mShakeAmountX = 0.0f;       // 峰值位移（原版符号约定）
	float mShakeAmountY = 0.0f;
	int   mShakeOscillations = 1;     // 1=原版三角弹跳；>1=衰减正弦来回甩

	void LoadSpawnListFromJson();
	void RefreshPlantStackRenderOrder(Cell* cell);
	void InitializeRows();
	inline int SelectSpawnRow(ZombieType type);
	bool IsSpawnRowCompatible(ZombieType type, int row) const;
	bool IsNaturalWaveSpawnRowCompatible(ZombieType type, int row) const;
	inline ZombieType PickZombieType(int remainingPoints);
	inline ZombieType GetWeightedRandomZombie();
	inline ZombieType GetCheapestZombie();
	void InitializeWeather();
	void UpdateWeather(float deltaTime);
	float GetWeatherLateGameFactor() const;
	float GetWeatherDirectorFactor() const;
	float GetWeatherTransitionProgress() const;
	float GetRainAudioVolume() const;
	void BeginWeatherTransition(RainIntensity target);
	void UpdateWeatherTransition(float deltaTime);
	void FinishWeatherTransitionImmediately();
	void RestoreWeatherTransition(RainIntensity previous, float remaining);
	int GetNextWeatherRollTotal() const;
	bool ShouldForceHeavyWeather() const;
	void RecordNewWeatherOutcome(RainIntensity next);
	RainIntensity RollNextWeather(int forcedRoll = 0);
	void PrepareWeatherForecast(int weatherRoll = 0);
	void ConsumeWeatherForecast();
	void PreparePendingHeavyTyphoon(int chanceRoll = 0, int strengthRoll = 0);
	void ClearPendingHeavyRainWarning();
	void MaybeShowHeavyRainPrompt();
	void BeginRain(RainIntensity intensity, float duration, bool canIntensify, bool canHold,
		bool allowTyphoonRoll = true);
	// 结束当前雨段：按固定权重落点决定放晴或进入一个不可再增强的尾雨段。
	void FinishRainPhase(int transitionRoll);
	void EndRain();
	void StartTyphoonForHeavyPhase(int chanceRoll = 0, int strengthRoll = 0,
		WindDirection forcedDirection = WindDirection::NONE);
	void ConsumePendingHeavyTyphoon();
	void StopTyphoon();
	void RestoreWeakWeatherPity(int weakWeatherPhases);
	void RestoreTyphoonPity(int missedHeavyPhases);
	void RestorePendingHeavyTyphoon(bool prepared, TyphoonStrength strength,
		WindDirection direction, float strengthTimer, float gustTimer,
		float directionTimer, int gustsRemaining, int promptVariant);
	void RestoreEliteDancerWaveSpawnCount(int count);
	void RestoreReinforcedDoorWaveSpawnCount(int count);
	void RestoreElitePolevaulterWaveSpawnCount(int count);
	void RestoreTyphoonState(TyphoonStrength strength, WindDirection direction,
		float strengthTimer, float gustTimer, float directionTimer, int gustsRemaining);
	void RestoreActiveTyphoonGust(bool active, TyphoonStrength strength,
		WindDirection direction, float duration, float remaining,
		float plantMoveRemaining, bool plantMoved);
	void UpdateTyphoon(float deltaTime);
	void UpdateTyphoonWindVisual(float deltaTime);
	void WeakenTyphoon();
	/** 到达维持时限后独立重抽风向；directionRoll=0 使用正式随机，1/2 供确定性测试。 */
	void RerollWindDirection(int directionRoll = 0);
	bool BeginTyphoonGust(bool consumeBudget, float forcedPlantMoveIn = -1.0f);
	void UpdateActiveTyphoonGust(float deltaTime);
	void EndTyphoonGust();
	void TriggerTyphoonPlantMove(TyphoonStrength strength, WindDirection direction);
	void EmitRainEffect(float duration);
	void RestartRainVisualForWindChange();
	void UpdateRainGroundSplash(float deltaTime);
	void TriggerRainGroundSplash();
	void StartRainAudio();
	void StopRainAudio();
	void RefreshZombieWeatherSpeeds();
	void TriggerLightning();
	// 生存模式"抽中权重"：对 NORMAL/CONE 随轮稀释(仅供 GetWeightedRandomZombie；成本侧仍用 GetZombieWeight)
	inline int GetSurvivalPickWeight(ZombieType type) const;

public:
	// 该行僵尸的视觉落脚 y（由地图行几何与地形美术偏移派生）。公开原因：伴舞出土裁剪要用
	// “行地面线”而非僵尸自身动态坐标定裁剪底边——换新地图/行高时自动适配（主人指示）。
	float GetZombieSpawnY(int row) const;
	/**
	 * 返回该行僵尸参与碰撞的逻辑基线 Y。
	 * 水路美术下沉不进入此坐标，避免子弹、植物和小推车判定随贴图对齐量漂移。
	 */
	float GetZombieCollisionY(int row) const;

	Board(GameScene* gameScene, Background background, int level);
	~Board();

	inline void AddSun(int amount)
	{
		// 只缩放正常收益入口；开局阳光、AutoTest set_sun 与花费均不走这里。
		const int scaledAmount = mPerkManager.ScaleSunIncome(amount);
		if (scaledAmount > MAX_SUN - mSun)
		{
			mSun = MAX_SUN;
			return;
		}
		mSun += scaledAmount;
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

	/** 当前雨势对僵尸 Animator extra 层的倍率。 */
	float GetZombieRainSpeedMultiplier() const;
	/** 独立天气压力进度（0～1）；供后期玩法倍率与天气导演共同使用。 */
	float GetWeatherPressureFactor() const;
	/** 台风对僵尸水平移动的额外倍率；返回值以当前雨天速度为 1。 */
	float GetZombieWindMoveMultiplier(bool movingTowardFront) const;
	/** 台风对轻型植物子弹水平速度的派生倍率；不修改子弹基础速度。 */
	float GetPlantBulletWindSpeedMultiplier(bool movingTowardFront) const;
	/** 台风对轻型植物子弹命中伤害的派生倍率；不修改子弹基础伤害。 */
	float GetPlantBulletWindDamageMultiplier(bool movingTowardFront) const;
	/** 当前雨势对植物攻击、生产、成长和恢复计时的倍率。 */
	float GetPlantRainActionSpeedMultiplier() const;
	/** 世界层蓝灰暗幕的 alpha（0..255）；UI 在暗幕之后绘制，不受影响。 */
	float GetRainOverlayAlpha() const;
	RainIntensity GetRainIntensity() const { return mRainIntensity; }
	RainIntensity GetPreviousRainIntensity() const { return mPreviousRainIntensity; }
	float GetWeatherTimer() const { return mWeatherTimer; }
	float GetWeatherTransitionTimer() const { return mWeatherTransitionTimer; }
	bool IsWeatherTransitionActive() const { return mWeatherTransitionTimer > 0.0f; }
	float GetLightningTimer() const { return mLightningTimer; }
	bool IsWeatherInitialized() const { return mWeatherInitialized; }
	bool CanRainIntensify() const { return mRainCanIntensify; }
	bool CanRainHold() const { return mRainCanHold; }
	bool HasWeatherForecast() const { return mWeatherForecastReady; }
	RainIntensity GetForecastRainIntensity() const { return mForecastRainIntensity; }
	RainIntensity GetActualForecastRainIntensity() const { return mActualForecastRainIntensity; }
	bool HasPendingHeavyTyphoon() const { return mPendingHeavyTyphoonPrepared; }
	TyphoonStrength GetPendingHeavyTyphoonStrength() const { return mPendingHeavyTyphoonStrength; }
	int GetPendingHeavyRainPromptVariant() const { return mPendingHeavyRainPromptVariant; }
	bool HasShownHeavyRainPrompt() const { return mHeavyRainPromptShown; }
	/** 当前天气预报准确率（百分比）；随导演强度成长但最高不超过 90%。 */
	int GetCurrentWeatherForecastAccuracyPercent() const;
	/** 当前连续非大雨新天气次数；只在后期天气导演启用时累计。 */
	int GetWeakWeatherPhasesSinceHeavy() const { return mWeakWeatherPhasesSinceHeavy; }
	/** 后期弱天气保底是否已经要求下一轮新天气为大雨。 */
	bool IsHeavyWeatherForced() const { return ShouldForceHeavyWeather(); }
	/** 当前天气导演下新天气的原始相对权重，不含弱天气保底覆盖。 */
	int GetCurrentNewWeatherWeight(RainIntensity intensity) const;
	bool HasTyphoon() const { return mTyphoonStrength != TyphoonStrength::NONE; }
	int GetCurrentTyphoonChancePercent() const;
	TyphoonStrength GetTyphoonStrength() const { return mTyphoonStrength; }
	WindDirection GetWindDirection() const { return mWindDirection; }
	float GetTyphoonStrengthTimer() const { return mTyphoonStrengthTimer; }
	float GetWindDirectionTimer() const { return mWindDirectionTimer; }
	float GetWindGustTimer() const { return mWindGustTimer; }
	int GetTyphoonGustsRemaining() const { return mTyphoonGustsRemaining; }
	bool IsTyphoonGustActive() const { return mTyphoonGustActive; }
	TyphoonStrength GetActiveGustStrength() const { return mActiveGustStrength; }
	WindDirection GetActiveGustDirection() const { return mActiveGustDirection; }
	float GetActiveGustDuration() const { return mActiveGustDuration; }
	float GetActiveGustTimer() const { return mActiveGustTimer; }
	float GetActiveGustPlantMoveTimer() const { return mActiveGustPlantMoveTimer; }
	bool HasActiveGustMovedPlants() const { return mActiveGustPlantMoved; }
	/** 返回当前阵风施加给全部存活僵尸的有符号水平漂移速度，正值吹向前线（像素/游戏秒）。 */
	float GetZombieGustDriftVelocity() const;
	/** 返回僵尸被阵风吹向前线时的最大世界横坐标，避免越过既有出生侧清理线。 */
	float GetZombieGustFrontLimit() const;
	int GetHeavyPhasesWithoutTyphoon() const { return mHeavyPhasesWithoutTyphoon; }
	int GetEliteDancersSpawnedThisWave() const { return mEliteDancersSpawnedThisWave; }
	int GetReinforcedDoorsSpawnedThisWave() const { return mReinforcedDoorsSpawnedThisWave; }
	int GetElitePolevaultersSpawnedThisWave() const { return mElitePolevaultersSpawnedThisWave; }
	int GetLastTyphoonMovedPlants() const { return mLastTyphoonMovedPlants; }
	int GetLastTyphoonLostPlants() const { return mLastTyphoonLostPlants; }
	bool IsTyphoonGustWarning() const;
	/** 当前公开预报是否属于此天气阶段真实允许出现的下一档。 */
	bool IsWeatherForecastPlausible() const;
	/** 当前雨势对应的粒子发射器是否仍在工作。 */
	bool IsRainEffectEmitting() const;
	/** 当前实际发射的雨丝配置名，供 AutoTest 精确断言风向切换。 */
	const std::string& GetRainVisualEffectName() const { return mRainVisualEffectName; }

	// AutoTest 专用：固定雨势并重启对应粒子，真实游戏只走随机天气状态机。
	void SetRainForTesting(RainIntensity intensity, float duration = 30.0f, bool canIntensify = false);
	// AutoTest 专用：定位黑夜无尽轮次，并刷新所有由轮次派生的天气速度状态。
	bool SetSurvivalRoundForTesting(int round);
	// AutoTest 专用：固定公开预报与真实天气，并把当前阶段倒计时改为指定揭晓时间。
	bool SetWeatherForecastForTesting(RainIntensity forecast, RainIntensity actual, float revealIn = 1.0f);
	// AutoTest 专用：覆盖已锁定大雨的待生效台风等级，验证四档预警文案与切档消费。
	bool SetPendingHeavyTyphoonForTesting(TyphoonStrength strength, int promptVariant = -1);
	// AutoTest 专用：在晴天用固定权重落点走正式新天气抽取，并发布必定准确的锁定预报。
	bool PrepareWeatherForecastForTesting(int weatherRoll, float revealIn = 0.1f);
	// AutoTest 专用：用固定权重落点结束当前雨段，覆盖增强、衰减和放晴分支。
	bool AdvanceRainPhaseForTesting(int transitionRoll);
	// AutoTest 专用：仅大雨允许触发，返回是否真正闪电。
	bool TriggerLightningForTesting();
	// AutoTest 专用：固定大雨附加台风、风向和计时，真实游戏只走阶段开始时的一次随机判定。
	bool SetTyphoonForTesting(TyphoonStrength strength, WindDirection direction,
		float gustIn = 30.0f, float directionIn = 30.0f, int gustsRemaining = 1,
		float decayIn = 30.0f);
	// AutoTest 专用：用固定二选一点数走正式风向重抽，覆盖保持同向与切换方向。
	bool RerollWindDirectionForTesting(int directionRoll);
	// AutoTest 专用：用固定概率点数和强度点数走正式台风判定，覆盖连续落空保底。
	bool RollTyphoonForTesting(int chanceRoll, int strengthRoll, WindDirection direction);
	// AutoTest 专用：启动一次当前强度的阵风，不消费自动预算；可固定植物结算时刻。
	bool TriggerTyphoonGustForTesting(float plantMoveIn = 0.0f);
	/** 正式波次与 AutoTest 共用的天气变异入口；mutationRoll=0 时随机，超额成功变异返回 NUM_ZOMBIE_TYPES。 */
	ZombieType ResolveRainMutationType(ZombieType selected, int mutationRoll = 0);
	/** 正式波次总解析入口；超过类型上限返回 NUM_ZOMBIE_TYPES，调用方必须跳过候选。 */
	ZombieType ResolveWaveZombieType(ZombieType selected, int mutationRoll = 0);

	// 初始化格子 默认5行9列
	void InitializeCell(int rows = 4, int cols = 8);
	/** 当前地图是否使用泳池地形与六行网格。 */
	bool IsPoolBackground() const;
	/** 天气从第二大关起启用；白天泳池同样受天气系统影响。 */
	bool SupportsWeather() const;
	bool IsPoolRow(int row) const;
	bool IsPoolSquare(int row, int col) const;
	bool IsPoolWorldPosition(int row, float x) const;
	/** 水路出怪扩展点：集中列出禁水类型；当前所有有效僵尸类型都允许进入水路。 */
	bool CanZombieTypeSpawnInPool(ZombieType type) const;
	Vector GetCellCenterPosition(int row, int col) const;
	float GetCellHeight() const { return mCellHeight; }

	/** UI 与测试共用的正式种植判定，不含阳光与卡片冷却。 */
	bool CanPlantAt(PlantType type, int row, int col);
	/** 返回该植物是否仍有本关种植次数；无限制的植物恒为 true。 */
	bool HasPlantingQuota(PlantType type) const;
	int GetEliteScaredyShroomPlantLimit() const;
	/** 返回格子最上层可被铲除或啃食的植物：普通层优先于承载层。 */
	Plant* GetTopPlantAt(int row, int col) const;
	/** 将基础僵尸按所选行解析为泳池表现变体；不改变波次成本。 */
	ZombieType ResolveTerrainZombieType(ZombieType selected, int row) const;
	bool CanSpawnZombieInRow(ZombieType type, int row) const {
		return IsSpawnRowCompatible(type, row);
	}

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

	// 创建樱桃爆炸效果；纵向范围按植物逻辑行覆盖相邻三行，避免泳池美术下沉干扰命中。
	void CreateBoom(const Vector& position, int plantRow, int damage = 1800);

	// 毁灭菇爆炸：半径 250 圆形判定、波及全部行、跳过魅惑僵尸；Charred 阈值逻辑同 CreateBoom
	void CreateDoomBoom(const Vector& position, int damage = 1800);

	// 添加/查询弹坑（毁灭菇）。AddCrater 由爆炸与读档共用；timeLeft 读档时传剩余值
	Crater* AddCrater(int row, int column, float timeLeft);
	bool HasCraterAt(int row, int column);   // 顺带惰性清理已消散的 weak_ptr

	// 屏幕抖动（移植原版 Board::ShakeBoard）。amountX/amountY 为峰值位移（像素，
	// 符号约定同原版：正 amountX 向左、正 amountY 向下）；oscillations=1 时为原版
	// 单次三角弹跳（0→满幅→0），>1 时改为衰减正弦来回甩（毁灭菇用）。
	// 计时随 dt 推进（暂停冻结）；纯视觉瞬态，不入存档。
	void ShakeBoard(float amountX, float amountY, float durationSeconds = 0.12f, int oscillations = 1);
	// 当前帧的抖动位移，GameScene::Draw 用它整体平移全部绘制命令；未抖动时恒 (0,0)
	Vector GetShakeOffset() const;

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
	/** 仅日间泳池调用：按独立节奏在随机水路位置生成 15 点小阳光。 */
	inline void UpdatePoolSunFalling(float deltaTime);

	/** 一次遍历刷新僵尸血量汇总与动态音乐所需的敌对僵尸数。 */
	inline void UpdateZombieMetrics();

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
	/** 保留指定小推车，静默移除其他现存小推车，不播放其启动动画或音效。 */
	void RemoveOtherMowersWithoutTrigger(int preservedMowerID);
};
#endif

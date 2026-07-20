#include "Board.h"
#include "../Logger.h"
#include "LawnMower.h"
#include "Shovel.h"
#include "Sun.h"
#include "Trophy.h"
#include "Crater.h"
#include "AdventureProgression.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Zombie/Zombie.h"

#include "EntityManager.h"
#include "RenderOrder.h"
#include "GameScene.h"
#include "AudioSystem.h"
#include "./Plant/GameDataManager.h"
#include "./GameProgress.h"
#include "../GameAPP.h"
#include "../FileManager.h"
#include "../ParticleSystem/ParticleSystem.h"
#include <unordered_set>
#include <climits>
#include <algorithm>   // std::max, std::swap
#include <cmath>       // std::lround

namespace {
	constexpr float kFirstRainDelayMin = 80.0f;          // 开局到首场雨的最短等待时间（秒）
	constexpr float kFirstRainDelayMax = 90.0f;          // 开局到首场雨的最长等待时间（秒）
	constexpr float kClearWeatherDelayMin = 15.0f;       // 两场雨之间的最短晴空间隔（秒）
	constexpr float kClearWeatherDelayMax = 40.0f;       // 两场雨之间的最长晴空间隔（秒）
	constexpr float kRainDurationMin = 85.0f;            // 一场新雨第一个雨段的最短持续时间（秒）
	constexpr float kRainDurationMax = 150.0f;            // 一场新雨第一个雨段的最长持续时间（秒）
	constexpr float kRainTailDurationMin = 45.0f;        // 雨势切档后尾雨段的最短持续时间（秒）
	constexpr float kRainTailDurationMax = 80.0f;        // 雨势切档后尾雨段的最长持续时间（秒）
	constexpr int kLightRainWeight = 45;                 // 小雨相对权重；数值越大越容易抽中
	constexpr int kMediumRainWeight = 40;                // 中雨相对权重；数值越大越容易抽中
	constexpr int kHeavyRainWeight = 32;                 // 大雨相对权重；数值越大越容易抽中
	constexpr int kRainWeightTotal = kLightRainWeight + kMediumRainWeight + kHeavyRainWeight; // 雨势总权重，单档改动后概率自动归一化
	constexpr int kLightToMediumWeight = 30;             // 初始小雨结束时增强为中雨的相对权重
	constexpr int kLightToHeavyWeight = 20;              // 初始小雨结束时骤增为大雨的相对权重
	constexpr int kLightToClearWeight = 45;              // 初始小雨结束时直接放晴的相对权重
	constexpr int kLightTransitionWeightTotal = kLightToMediumWeight + kLightToHeavyWeight + kLightToClearWeight; // 初始小雨转档总权重
	constexpr int kMediumToLightWeight = 35;             // 中雨结束时衰减为小雨的相对权重
	constexpr int kMediumToClearWeight = 65;             // 中雨结束时直接放晴的相对权重
	constexpr int kMediumTransitionWeightTotal = kMediumToLightWeight + kMediumToClearWeight; // 中雨转档总权重
	constexpr int kHeavyToMediumWeight = 45;             // 大雨结束时衰减为中雨的相对权重
	constexpr int kHeavyToLightWeight = 20;              // 大雨结束时直接衰减为小雨的相对权重
	constexpr int kHeavyToClearWeight = 35;              // 大雨结束时直接放晴的相对权重
	constexpr int kHeavyTransitionWeightTotal = kHeavyToMediumWeight + kHeavyToLightWeight + kHeavyToClearWeight; // 大雨转档总权重
	constexpr float kLightZombieSpeed = 1.15f;           // 小雨僵尸动作与移动速度倍率
	constexpr float kMediumZombieSpeed = 1.25f;          // 中雨僵尸动作与移动速度倍率
	constexpr float kHeavyZombieSpeed = 1.40f;           // 大雨僵尸动作与移动速度倍率
	constexpr float kLightPlantActionSpeed = 1.10f;      // 小雨植物攻击、生产、成长与恢复速度倍率
	constexpr float kMediumPlantActionSpeed = 1.15f;     // 中雨植物攻击、生产、成长与恢复速度倍率
	constexpr float kHeavyPlantActionSpeed = 1.20f;      // 大雨植物攻击、生产、成长与恢复速度倍率
	constexpr float kLightOverlayAlpha = 30.0f;          // 小雨世界蓝灰暗幕透明度（0～255）
	constexpr float kMediumOverlayAlpha = 50.0f;         // 中雨世界蓝灰暗幕透明度（0～255）
	constexpr float kHeavyOverlayAlpha = 80.0f;          // 大雨世界蓝灰暗幕透明度（0～255）
	constexpr float kLightRainVolume = 0.20f;            // 小雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kMediumRainVolume = 0.28f;           // 中雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kHeavyRainVolume = 0.48f;            // 大雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kLightSplashDelayMin = 0.16f;         // 小雨两次地面水花的最短间隔（秒，约每秒 5 次）
	constexpr float kLightSplashDelayMax = 0.24f;         // 小雨两次地面水花的最长间隔（秒，约每秒 5 次）
	constexpr float kMediumSplashDelayMin = 0.08f;        // 中雨两次地面水花的最短间隔（秒，约每秒 10 次）
	constexpr float kMediumSplashDelayMax = 0.12f;        // 中雨两次地面水花的最长间隔（秒，约每秒 10 次）
	constexpr float kHeavySplashDelayMin = 0.04f;         // 大雨两次地面水花的最短间隔（秒，约每秒 20 次）
	constexpr float kHeavySplashDelayMax = 0.06f;         // 大雨两次地面水花的最长间隔（秒，约每秒 20 次）
	constexpr float kRainSplashEdgePadding = 18.0f;       // 水花中心距草地网格边缘的安全距离（像素）
	constexpr float kLightningDelayMin = 3.5f;           // 大雨开始后首次闪电的最短等待时间（秒）
	constexpr float kLightningDelayMax = 7.0f;           // 大雨开始后首次闪电的最长等待时间（秒）
	constexpr float kLightningRepeatMin = 5.0f;          // 大雨中两次闪电的最短间隔（秒）
	constexpr float kLightningRepeatMax = 10.0f;         // 大雨中两次闪电的最长间隔（秒）
	constexpr float kLightningFlashDuration = 0.18f;     // 单次闪电白闪持续时间（秒）
	constexpr float kLightningFlashPeakAlpha = 105.0f;   // 单次闪电白闪峰值透明度（0～255）

	const char* RainEffectName(RainIntensity intensity)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:  return "RainLight";
		case RainIntensity::MEDIUM: return "RainMedium";
		case RainIntensity::HEAVY:  return "RainHeavy";
		case RainIntensity::CLEAR:  break;
		}
		return "";
	}

	/** 按当前雨势抽取下一次地面水花间隔；雨越大，水花越频繁。 */
	float RandomRainSplashDelay(RainIntensity intensity)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:
			return GameRandom::Range(kLightSplashDelayMin, kLightSplashDelayMax);
		case RainIntensity::MEDIUM:
			return GameRandom::Range(kMediumSplashDelayMin, kMediumSplashDelayMax);
		case RainIntensity::HEAVY:
			return GameRandom::Range(kHeavySplashDelayMin, kHeavySplashDelayMax);
		case RainIntensity::CLEAR:
			return 0.0f;
		}
		return 0.0f;
	}

	int RainTransitionWeightTotal(RainIntensity intensity, bool canIntensify)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:  return canIntensify ? kLightTransitionWeightTotal : 0;
		case RainIntensity::MEDIUM: return kMediumTransitionWeightTotal;
		case RainIntensity::HEAVY:  return kHeavyTransitionWeightTotal;
		case RainIntensity::CLEAR:  return 0;
		}
		return 0;
	}

	// 把一次权重落点解析为下一雨势。只有一场雨最初的小雨能增强；切档后只会继续衰减。
	RainIntensity RainTransitionForRoll(RainIntensity intensity, bool canIntensify, int roll)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:
			if (!canIntensify) return RainIntensity::CLEAR;
			if (roll <= kLightToMediumWeight) return RainIntensity::MEDIUM;
			if (roll <= kLightToMediumWeight + kLightToHeavyWeight) return RainIntensity::HEAVY;
			return RainIntensity::CLEAR;
		case RainIntensity::MEDIUM:
			return roll <= kMediumToLightWeight ? RainIntensity::LIGHT : RainIntensity::CLEAR;
		case RainIntensity::HEAVY:
			if (roll <= kHeavyToMediumWeight) return RainIntensity::MEDIUM;
			if (roll <= kHeavyToMediumWeight + kHeavyToLightWeight) return RainIntensity::LIGHT;
			return RainIntensity::CLEAR;
		case RainIntensity::CLEAR:
			return RainIntensity::CLEAR;
		}
		return RainIntensity::CLEAR;
	}
}

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
		int mBigLevel = AdventureProgression::GetAreaNumber(mLevel);
		int mSmallLevel = AdventureProgression::GetLevelNumberInArea(mLevel);
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

Board::~Board()
{
	// 原版 Board.DisposeBoard 无条件 StopFoley(Rain)；离开关卡时同样保证循环声不泄漏到菜单。
	StopRainAudio();
}

float Board::GetZombieRainSpeedMultiplier() const
{
	switch (mRainIntensity) {
	case RainIntensity::LIGHT:  return kLightZombieSpeed;
	case RainIntensity::MEDIUM: return kMediumZombieSpeed;
	case RainIntensity::HEAVY:  return kHeavyZombieSpeed;
	case RainIntensity::CLEAR:  return 1.0f;
	}
	return 1.0f;
}

float Board::GetPlantRainActionSpeedMultiplier() const
{
	switch (mRainIntensity) {
	case RainIntensity::LIGHT:  return kLightPlantActionSpeed;
	case RainIntensity::MEDIUM: return kMediumPlantActionSpeed;
	case RainIntensity::HEAVY:  return kHeavyPlantActionSpeed;
	case RainIntensity::CLEAR:  return 1.0f;
	}
	return 1.0f;
}

float Board::GetRainOverlayAlpha() const
{
	switch (mRainIntensity) {
	case RainIntensity::LIGHT:  return kLightOverlayAlpha;
	case RainIntensity::MEDIUM: return kMediumOverlayAlpha;
	case RainIntensity::HEAVY:  return kHeavyOverlayAlpha;
	case RainIntensity::CLEAR:  return 0.0f;
	}
	return 0.0f;
}

void Board::InitializeWeather()
{
	if (mWeatherInitialized) return;
	// 主进度由“当前雨势 + 一个复用倒计时”驱动：CLEAR 时倒计时代表距首场雨/下一场雨，
	// 下雨时则代表当前雨段剩余时间；另用布尔值记录初始小雨是否还拥有一次增强资格。
	// 白天把倒计时保持为 0，并由 UpdateWeather 直接跳过。
	mWeatherInitialized = true;
	mRainIntensity = RainIntensity::CLEAR;
	mLightningTimer = 0.0f;
	mRainSplashTimer = 0.0f;
	mRainCanIntensify = false;
	mRainVisualActive = false;
	mWeatherTimer = GameAPP::GetInstance().GetBackgroundIsNight(mBackGround)
		? GameRandom::Range(kFirstRainDelayMin, kFirstRainDelayMax)
		: 0.0f;
}

void Board::RefreshZombieWeatherSpeeds()
{
	for (int id : mEntityManager.GetAllZombieIDs()) {
		Zombie* zombie = mEntityManager.GetZombie(id);
		if (zombie) zombie->RefreshAnimSpeedForWeather();
	}
}

void Board::EmitRainEffect(float duration)
{
	if (!g_particleSystem || mRainIntensity == RainIntensity::CLEAR || duration <= 0.0f) return;
	// Box 发射器以屏幕上沿中央为基准铺满逻辑画面；运行期时长覆盖 XML 上限，
	// 使随机雨长与读档剩余时间都能和玩法倍率同步结束。
	g_particleSystem->EmitEffect(RainEffectName(mRainIntensity),
		Vector(static_cast<float>(SCENE_WIDTH) * 0.5f, -60.0f),
		LAYER_EFFECTS_WORLD, duration);
	mRainVisualActive = true;
}

/** 在草地逻辑网格内随机选择落点，播放短促的原版雨滴水花与扩散圆圈。 */
void Board::TriggerRainGroundSplash()
{
	if (!g_particleSystem || mRows <= 0 || mColumns <= 0) return;

	// 用完整网格边界而非窗口随机值，既覆盖战场又给 33px 水花留下屏内余量。
	const float minX = CELL_INITALIZE_POS_X + kRainSplashEdgePadding;
	const float maxX = CELL_INITALIZE_POS_X + mColumns * CELL_COLLIDER_SIZE_X
		- kRainSplashEdgePadding;
	const float minY = CELL_INITALIZE_POS_Y + kRainSplashEdgePadding;
	const float maxY = CELL_INITALIZE_POS_Y + mRows * CELL_COLLIDER_SIZE_Y
		- kRainSplashEdgePadding;
	if (maxX <= minX || maxY <= minY) return;

	g_particleSystem->EmitEffect("RainGroundSplash",
		Vector(GameRandom::Range(minX, maxX), GameRandom::Range(minY, maxY)),
		LAYER_EFFECTS_WORLD);
}

/** 推进地面水花节奏；计时器是纯视觉状态，雨势切换和读档后都会重新起拍。 */
void Board::UpdateRainGroundSplash(float deltaTime)
{
	if (mRainIntensity == RainIntensity::CLEAR) return;
	mRainSplashTimer -= deltaTime;
	if (mRainSplashTimer > 0.0f) return;

	TriggerRainGroundSplash();
	mRainSplashTimer = RandomRainSplashDelay(mRainIntensity);
}

bool Board::IsRainEffectEmitting() const
{
	return g_particleSystem && mRainIntensity != RainIntensity::CLEAR
		&& g_particleSystem->IsEffectEmitting(RainEffectName(mRainIntensity));
}

void Board::StartRainAudio()
{
	float volume = kLightRainVolume;
	switch (mRainIntensity) {
	case RainIntensity::LIGHT:  volume = kLightRainVolume; break;
	case RainIntensity::MEDIUM: volume = kMediumRainVolume; break;
	case RainIntensity::HEAVY:  volume = kHeavyRainVolume; break;
	case RainIntensity::CLEAR:  return;
	}
	// 原版 FoleyType.Rain 绑定 SOUND_RAIN 且带 Loop 标志；这里只把音量按雨势分层。
	AudioSystem::PlayLoopingSound(ResourceKeys::Sounds::SOUND_RAIN, volume);
}

void Board::StopRainAudio()
{
	AudioSystem::StopLoopingSound(ResourceKeys::Sounds::SOUND_RAIN);
}

void Board::BeginRain(RainIntensity intensity, float duration, bool canIntensify)
{
	if (intensity == RainIntensity::CLEAR || duration <= 0.0f) return;
	mRainIntensity = intensity;
	mWeatherTimer = duration;
	mRainCanIntensify = canIntensify && intensity == RainIntensity::LIGHT;
	mRainSplashTimer = RandomRainSplashDelay(intensity);
	mLightningTimer = (intensity == RainIntensity::HEAVY)
		? GameRandom::Range(kLightningDelayMin, kLightningDelayMax)
		: 0.0f;
	mRainVisualActive = false;
	RefreshZombieWeatherSpeeds();
	EmitRainEffect(duration);
	StartRainAudio();
}

void Board::FinishRainPhase(int transitionRoll)
{
	const RainIntensity next = RainTransitionForRoll(
		mRainIntensity, mRainCanIntensify, transitionRoll);
	if (next == RainIntensity::CLEAR) {
		EndRain();
		return;
	}

	// 切档后的雨段统一进入衰减链，不再拥有增强资格，因此状态转移必然在有限步内放晴。
	BeginRain(next, GameRandom::Range(kRainTailDurationMin, kRainTailDurationMax), false);
}

void Board::EndRain()
{
	StopRainAudio();
	mRainIntensity = RainIntensity::CLEAR;
	mWeatherTimer = GameRandom::Range(kClearWeatherDelayMin, kClearWeatherDelayMax);
	mLightningTimer = 0.0f;
	mRainSplashTimer = 0.0f;
	mRainCanIntensify = false;
	mRainVisualActive = false;
	RefreshZombieWeatherSpeeds();
}

void Board::TriggerLightning()
{
	if (mRainIntensity != RainIntensity::HEAVY || !mGameScene) return;
	// 仅做短促低峰值白闪；不启用动态光源、阴影或材质高光。
	mGameScene->ShowScreenFlash(kLightningFlashDuration, kLightningFlashPeakAlpha);
}

void Board::UpdateWeather(float deltaTime)
{
	if (!mWeatherInitialized || deltaTime <= 0.0f ||
		!GameAPP::GetInstance().GetBackgroundIsNight(mBackGround)) return;

	// 每帧只推进当前阶段的倒计时。雨中归零会按当前强度决定增强、衰减或放晴；
	// CLEAR 阶段归零则抽取一场新雨，形成有界的 CLEAR→RAIN(可多段)→CLEAR 循环。
	mWeatherTimer -= deltaTime;
	if (mRainIntensity != RainIntensity::CLEAR && mWeatherTimer > 0.0f
		&& !IsRainEffectEmitting()) {
		// 粒子系统若因场景清理或异常耗尽而与天气状态脱节，用剩余时长自动补发。
		mRainVisualActive = false;
		EmitRainEffect(mWeatherTimer);
	}
	if (mRainIntensity != RainIntensity::CLEAR && mWeatherTimer > 0.0f) {
		UpdateRainGroundSplash(deltaTime);
	}
	if (mRainIntensity == RainIntensity::HEAVY) {
		mLightningTimer -= deltaTime;
		if (mLightningTimer <= 0.0f) {
			TriggerLightning();
			mLightningTimer = GameRandom::Range(kLightningRepeatMin, kLightningRepeatMax);
		}
	}

	if (mWeatherTimer > 0.0f) return;
	if (mRainIntensity != RainIntensity::CLEAR) {
		const int total = RainTransitionWeightTotal(mRainIntensity, mRainCanIntensify);
		if (total > 0) FinishRainPhase(GameRandom::Range(1, total));
		else EndRain();
		return;
	}

	// 在总权重内掷一次整数骰子：依次落入小雨、中雨、大雨各自的权重区间。
	// 初始小雨可能增强；初始中/大雨及所有切档后的雨段只会衰减，避免无限来回循环。
	const int roll = GameRandom::Range(1, kRainWeightTotal);
	const RainIntensity next = (roll <= kLightRainWeight) ? RainIntensity::LIGHT
		: (roll <= kLightRainWeight + kMediumRainWeight) ? RainIntensity::MEDIUM
		: RainIntensity::HEAVY;
	BeginRain(next, GameRandom::Range(kRainDurationMin, kRainDurationMax), next == RainIntensity::LIGHT);
}

void Board::SetRainForTesting(RainIntensity intensity, float duration, bool canIntensify)
{
	if (!GameAPP::GetInstance().GetBackgroundIsNight(mBackGround)) return;
	if (g_particleSystem) g_particleSystem->ClearAll();
	StopRainAudio();
	mWeatherInitialized = true;
	mRainVisualActive = false;

	if (intensity == RainIntensity::CLEAR) {
		mRainIntensity = RainIntensity::CLEAR;
		mWeatherTimer = std::max(duration, 0.1f);
		mLightningTimer = 0.0f;
		mRainSplashTimer = 0.0f;
		mRainCanIntensify = false;
		RefreshZombieWeatherSpeeds();
		return;
	}
	BeginRain(intensity, std::max(duration, 0.1f), canIntensify);
}

bool Board::AdvanceRainPhaseForTesting(int transitionRoll)
{
	if (mRainIntensity == RainIntensity::CLEAR) return false;
	const int total = RainTransitionWeightTotal(mRainIntensity, mRainCanIntensify);
	if (total > 0 && (transitionRoll < 1 || transitionRoll > total)) return false;

	// 测试会在雨段尚未自然到期时强制切档；先清旧雨丝，模拟生产路径中旧发射器已到期。
	if (g_particleSystem) g_particleSystem->ClearAll();
	mRainVisualActive = false;
	if (total > 0) FinishRainPhase(transitionRoll);
	else EndRain();
	return true;
}

bool Board::TriggerLightningForTesting()
{
	if (mRainIntensity != RainIntensity::HEAVY) return false;
	TriggerLightning();
	return true;
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
	// 植物爆炸明确标记为 PLANT；Zombie::TakeDamage 只对该来源应用植物增伤，再统一应用僵尸免伤。
	// 这里仅预测 TakeDamage 实际造成的伤害，用于秒杀(Charred)阈值判定——保持判定与扣血一致，
	// 且不在入口重复缩放（旧代码此处 ScalePlantDamage 一次、TakeDamage 又缩放一次 → 词条被算两遍）。
	const int scaledDamage = mPerkManager.ScaleTotalDamageToZombie(damage);
	g_particleSystem->EmitEffect("CherryBomb", position);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CHERRYBOMB, 0.4f);
	ShakeBoard(3.0f, -4.0f);   // 原版 ShakeBoard(3,-4)：0.12s 单次弹跳
	std::vector<int> zombieIDs = mEntityManager.GetAllZombieIDs();
	for (auto zombieID : zombieIDs)
	{
		if (auto zombie = mEntityManager.GetZombie(zombieID)) {
			if (zombie->IsMindControlled()) continue;
			Vector zombiePositon = zombie->GetPosition();
			if (std::abs(zombiePositon.x - position.x) <= 130.0f &&
				std::abs(zombiePositon.y - position.y) <= 130.0f)
			{
				if (zombie->mBodyHealth <= scaledDamage)
				{
					zombie->Charred();
				}
				else
				{
					zombie->TakeDamage(damage, DamageSource::PLANT);   // 传未缩放原值，TakeDamage 内部缩放一次
				}
			}
		}
	}
}

void Board::CreateDoomBoom(const Vector& position, int damage)
{
	// 与 CreateBoom 同一套 Charred 阈值预测（词条缩放单点在 TakeDamage，勿在入口重复缩放）
	const int scaledDamage = mPerkManager.ScaleTotalDamageToZombie(damage);
	g_particleSystem->EmitEffect("Doom", position);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_DOOMSHROOM, 0.5f);
	// 比樱桃更剧烈：双倍振幅 + 0.5s 衰减正弦来回甩 5 个半周期（原版两者同为 3,-4，主人要求毁灭菇加强）
	ShakeBoard(6.0f, -9.0f, 0.5f, 5);
	std::vector<int> zombieIDs = mEntityManager.GetAllZombieIDs();
	for (auto zombieID : zombieIDs)
	{
		if (auto zombie = mEntityManager.GetZombie(zombieID)) {
			if (zombie->IsMindControlled()) continue;
			// 圆(半径 250) vs 僵尸判定矩形 [x±25]×[y-65,y+35]，镜像原版 GetCircleRectOverlap；
			// 250 纵向天然覆盖 ±2 行有余，无需再按行数过滤
			Vector zombiePosition = zombie->GetPosition();
			float nearestX = std::clamp(position.x, zombiePosition.x - 25.0f, zombiePosition.x + 25.0f);
			float nearestY = std::clamp(position.y, zombiePosition.y - 65.0f, zombiePosition.y + 35.0f);
			float dx = position.x - nearestX;
			float dy = position.y - nearestY;
			if (dx * dx + dy * dy <= 250.0f * 250.0f)
			{
				if (zombie->mBodyHealth <= scaledDamage)
				{
					zombie->Charred();
				}
				else
				{
					zombie->TakeDamage(damage, DamageSource::PLANT);   // 传未缩放原值，TakeDamage 内部缩放一次
				}
			}
		}
	}
}

void Board::ShakeBoard(float amountX, float amountY, float durationSeconds, int oscillations)
{
	mShakeDuration = (durationSeconds > 0.0f) ? durationSeconds : 0.12f;
	mShakeTimer = mShakeDuration;
	mShakeAmountX = amountX;
	mShakeAmountY = amountY;
	mShakeOscillations = (oscillations > 0) ? oscillations : 1;
}

Vector Board::GetShakeOffset() const
{
	if (mShakeTimer <= 0.0f) return Vector(0.0f, 0.0f);
	float t = 1.0f - mShakeTimer / mShakeDuration;   // 0→1
	float wave;
	if (mShakeOscillations <= 1) {
		// 原版 TodCurves.Bounce：三角波 0→1→0（峰值在半程）
		wave = 1.0f - std::abs(1.0f - t * 2.0f);
	}
	else {
		// 衰减正弦：sin 每半周期变号=方向来回甩，(1-t) 包络收敛回原位
		wave = std::sin(t * 3.14159265f * static_cast<float>(mShakeOscillations)) * (1.0f - t);
	}
	// 原版符号：mX = base - amountX*wave（正 amountX 向左）、mY = 0 + amountY*wave
	return Vector(-mShakeAmountX * wave, mShakeAmountY * wave);
}

Crater* Board::AddCrater(int row, int column, float timeLeft)
{
	auto crater = GameObjectManager::GetInstance().CreateGameObjectAsShared<Crater>(
		LAYER_GAME_OBJECT, this, row, column, timeLeft);
	if (crater) {
		mCraters.push_back(crater);
	}
	return crater.get();
}

bool Board::HasCraterAt(int row, int column)
{
	bool found = false;
	// 弹坑到时在自身 Update 里自毁，这里顺带收缩失效的 weak_ptr
	mCraters.erase(std::remove_if(mCraters.begin(), mCraters.end(),
		[&](const std::weak_ptr<Crater>& weak) {
			auto crater = weak.lock();
			if (!crater || !crater->IsActive()) return true;
			if (crater->mRow == row && crater->mColumn == column) found = true;
			return false;
		}), mCraters.end());
	return found;
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
	mTrophy = trophy;
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

	// 开发者面板「暂停刷怪」：冻结出波倒计时与本波清空提前出波，SummonNextWave 直调入口不受影响
	if (GameAPP::mDevSpawnPaused)
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
			// 原版在一大波警告期间强制进入 burst。黑夜曲的鼓轨是独立段落，
			// 更早切入；其余场景等警告牌展示一段时间后再在小节边界加入鼓组。
			const float burstDelay = (mBackGround == Background::GROUND_NIGHT) ? 0.5f : 3.5f;
			if (!mHasHugeWaveMusicBurst && mHugeWaveCountDown >= burstDelay)
			{
				mHasHugeWaveMusicBurst = true;
				AudioSystem::StartMusicBurst();
			}
			if (mHugeWaveCountDown >= 7.5f)
			{
				mHugeWaveCountDown = 0.0f;
				mHasHugeWaveSound = false;
				mHasHugeWaveMusicBurst = false;
			}
			else
			{
				return;
			}
		}
		SummonNextWave();
	}
}

// 推进并生成下一波（波次+1、首波/最后一波/大波音效提示、生成僵尸、波血量记账）。
// 由 Update 出波倒计时归零调用；开发者面板「下一波」也直接调用（暂停中同样可出波）。
void Board::SummonNextWave()
{
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

	mNextWaveSpawnZombieHP = static_cast<int64_t>
		(GameRandom::Range(0.5f, 0.65f) * static_cast<double>(mCurrectWaveZombieHP));
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

inline int Board::GetSurvivalPickWeight(ZombieType type) const
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
	float x = static_cast<float>(SCENE_WIDTH) + 40;

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
	int64_t TotalHP = 0, CurrectWaveHP = 0;
	for (auto zombieID : mEntityManager.GetAllZombieIDs())
	{
		if (auto zombie = mEntityManager.GetZombie(zombieID))
		{
			if (zombie->IsMindControlled()) continue;	// 判断是不是魅惑

			int64_t zombieHp = static_cast<int64_t>(zombie->mBodyHealth) +
				static_cast<int64_t>(zombie->mHelmHealth) + static_cast<int64_t>(zombie->mShieldHealth);

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
	// 节拍帧随游戏时间而非逻辑步推进：暂停时逻辑步照跑（UI 要消费点击）但 dt=0，
	// 若无条件 ++，暂停中舞王/伴舞会随节拍翻转瞬间切轨；倍速下也与 Animator 的缩放 dt 同步。
	mBoardFrameAccum += DeltaTime::GetDeltaTime() / DeltaTime::GetFixedStep();
	while (mBoardFrameAccum >= 1.0f) {
		mBoardFrameAccum -= 1.0f;
		mBoardFrame++;
	}
	// 屏幕抖动倒计时：乘 dt 口径（暂停 dt=0 冻结，倍速等比加速），与弹坑计时一致
	if (mShakeTimer > 0.0f) {
		mShakeTimer -= DeltaTime::GetDeltaTime();
	}
	// 天气属于整片场景而非波次逻辑：生存轮间也自然推进；暂停时 dt=0 与粒子同步冻结。
	UpdateWeather(DeltaTime::GetDeltaTime());
	CleanupExpiredObjects();
	UpdateLevel();
	AudioSystem::UpdateAdaptiveMusic(DeltaTime::GetDeltaTime(), CountHostileZombiesForMusic());
}

int Board::CountHostileZombiesForMusic() const
{
	int count = 0;
	for (int zombieID : mEntityManager.GetAllZombieIDs())
	{
		Zombie* zombie = mEntityManager.GetZombie(zombieID);
		if (!zombie || zombie->IsDying() || !zombie->HasHead() ||
			zombie->IsMindControlled()) continue;
		++count;
	}
	return count;
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
	InitializeWeather();
	// 读档恢复到一场雨中时，玩法状态已经由存档还原；粒子是瞬态资源，需按剩余时间重建一次。
	if (mRainIntensity != RainIntensity::CLEAR && !mRainVisualActive)
	{
		EmitRainEffect(mWeatherTimer);
		StartRainAudio();
	}

	PlayBackgroundMusic();
}

void Board::PlayBackgroundMusic()
{
	switch (mBackGround)
	{
	case Background::GROUND_DAY:
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_DAY, -1);
		break;
	case Background::GROUND_NIGHT:
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_NIGHT, -1);
		break;
	case Background::WATER_POOL:
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_POOL, -1);
		break;
	case Background::NIGHT_WATER_POOL:
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_FOG, -1);
		break;
	case Background::ROOF:
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_ROOF, -1);
		break;
	case Background::NIGHT_ROOF:
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_NIGHT, -1);
		break;
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
	mTrophy.reset();
	mHasHugeWaveSound = false;
	mHasHugeWaveMusicBurst = false;
	mHugeWaveCountDown = 0.0f;
	mNextWaveSpawnZombieHP = 0;
	mCurrectWaveZombieHP = 0;
	mTotalZombieHP = 0;

	// 重算难度（解锁更强僵尸）+ 刷新关卡名
	BuildSurvivalSpawnList(mSurvivalRound);
	UpdateSurvivalLevelName();

	// 回到选卡：暂停波次推进
	mBoardState = BoardState::CHOOSE_CARD;

	// 通知场景：先结算两次成对词条机会（每次可选或放弃），然后再链式进入选卡。
	if (mGameScene)
		mGameScene->BeginSurvivalPerkSelect();
}

void Board::BuildSurvivalSpawnList(int round)
{
	mSpawnZombieList.clear();
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);   // 普通：每轮必有，先固定放入

	// 1) 收集本轮合格候选(排除普通)。旗数递减下调每种僵尸的"有效最早轮次"。
	int flagsCompleted = round - 1;
	int reduction = SurvivalCurveLerp(SURVIVAL_UNLOCK_REDUCE_START_FLAG,
	                                  SURVIVAL_UNLOCK_REDUCE_END_FLAG,
	                                  flagsCompleted, 0, SURVIVAL_UNLOCK_REDUCE_MAX);
	std::vector<ZombieType> candidates;
	for (int i = 0; i < static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES); ++i)
	{
		ZombieType t = static_cast<ZombieType>(i);
		if (t == ZombieType::ZOMBIE_NORMAL) continue;
		// 粉色橄榄球是黑夜专属变体；冒险关由 spawnlists.json 控制，这里只隔离两种无尽模式。
		if (t == ZombieType::ZOMBIE_PINK_FOOTBALL
			&& mLevel != SURVIVAL_ENDLESS_NIGHT_LEVEL) continue;
		int base = GameDataManager::GetInstance().GetZombieSurvivalRound(t);
		if (base < 1) continue;                              // 0 = 不进生存
		if (GameDataManager::GetInstance().GetZombieWeight(t) <= 0) continue; // 伴舞等召唤单位不独立占池位
		int eff = std::max(base - reduction, 1);
		if (eff <= round) candidates.push_back(t);
	}

	// 2) 第 1~2 轮：候选全放(确定性)→ 自然得到 {普通} / {普通,路障}
	if (round < SURVIVAL_RANDOM_POOL_START_ROUND)
	{
		for (ZombieType t : candidates) mSpawnZombieList.push_back(t);
		return;
	}

	// 3) 第 3 轮起：基础总种类缓慢增长并先钳到 8，再随机 ±1~2 种。
	//    深轮正波动留在上限，负波动形成 6~7 种，避免所有已实现类型每轮固定全上。
	const int baseExtra = SURVIVAL_POOL_BASE_EXTRA
		+ (round - SURVIVAL_RANDOM_POOL_START_ROUND) / SURVIVAL_POOL_GROWTH_EVERY;
	const int maxTypes = std::min(SURVIVAL_POOL_MAX_TYPES,
		static_cast<int>(candidates.size()) + 1);
	const int baseTypes = std::min(1 + baseExtra, maxTypes);
	const int jitterMagnitude = GameRandom::Range(SURVIVAL_POOL_JITTER_MIN, SURVIVAL_POOL_JITTER_MAX);
	const int jitter = GameRandom::Range(0, 1) == 0 ? -jitterMagnitude : jitterMagnitude;
	const int minTypes = std::min(2, maxTypes); // 第3轮起有候选时至少保留普通+1种
	const int targetTypes = std::clamp(baseTypes + jitter, minTypes, maxTypes);
	const int extra = targetTypes - 1;

	// 预筛合格者后做部分 Fisher-Yates，无放回且无需重抽循环。
	for (int k = 0; k < extra; ++k)
	{
		int j = GameRandom::Range(k, static_cast<int>(candidates.size()) - 1);
		std::swap(candidates[k], candidates[j]);
		mSpawnZombieList.push_back(candidates[k]);
	}
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
		// 可选：每关初始阳光（如黑夜收官关 19 给 1500）。只在开新关生效——
		// 读档路径在本函数之后由 GameInfoSaver 还原存档里的 mSun，天然覆盖。
		if (entry.contains("sun"))
		{
			int sun = entry["sun"].get<int>();
			if (sun > 0)
				mSun = sun;
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

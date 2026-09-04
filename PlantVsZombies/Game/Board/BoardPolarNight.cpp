#include "Game/Board/Board.h"
#include "Game/AdventureProgression.h"
#include "Game/AudioSystem.h"
#include "Game/Plant/Plant.h"
#include "Game/Zombie/Zombie.h"
#include "GameApp.h"
#include "GameRandom.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "ParticleSystem/ParticleSystem.h"
#include "Graphics.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {
	constexpr float kPolarBaselineTemperatureC = -14.0f; // 极夜每轮无危险起点温度，单位摄氏度
	constexpr float kPolarBaselineHumidity = 58.0f;      // 极夜每轮无危险起点相对湿度，百分比
	constexpr float kPolarBaselineWindMps = 8.0f;        // 极夜每轮无危险起点风速，米/秒
	constexpr float kPolarTemperatureDangerC = -18.0f;   // 温度仪危险阈值，摄氏度
	constexpr float kPolarHumidityDanger = 85.0f;        // 湿度仪危险阈值，百分比
	constexpr float kPolarWindDangerMps = 18.0f;         // 风速仪危险阈值，米/秒
	constexpr float kPolarWhiteoutCommitSeconds = 5.0f;  // 三项连续危险到不可逆提交的游戏秒
	constexpr float kPolarHoleCommitSeconds = 3.0f;      // 高湿连续到雪穴选点的游戏秒
	constexpr float kPolarHoleFormationSeconds = 2.0f;   // 雪堆占位到雪穴活动态的游戏秒
	constexpr float kPolarWhiteoutRampSeconds = 1.5f;    // 提交后音画爬升时长，游戏秒
	constexpr float kPolarSnowBlindSeconds = 45.0f;      // 普通极夜关雪盲持续时间，游戏秒
	constexpr float kPolarFinalSnowBlindSeconds = 60.0f; // 8-9 白毛风雪盲持续时间，游戏秒
	constexpr float kPolarWhiteoutFadeSeconds = 2.0f;    // 玩法解除后的无害音画淡出，游戏秒
	constexpr float kPolarPostWhiteoutRecoverySeconds = 5.0f; // 白毛风结束后三仪表连续回落到常态的游戏秒
	constexpr float kPolarFluctuationDurationMin = 1.8f; // 仪表单段微波动最短时长，游戏秒
	constexpr float kPolarFluctuationDurationMax = 3.2f; // 仪表单段微波动最长时长，游戏秒
	constexpr float kPolarTemperatureFluctuationMin = 0.6f; // 每段温度微波动最小幅度，摄氏度
	constexpr float kPolarTemperatureFluctuationMax = 1.2f; // 每段温度微波动最大幅度，摄氏度
	constexpr float kPolarHumidityFluctuationMin = 1.2f; // 每段湿度微波动最小幅度，百分点
	constexpr float kPolarHumidityFluctuationMax = 2.8f; // 每段湿度微波动最大幅度，百分点
	constexpr float kPolarWindFluctuationMin = 0.7f;    // 每段风速微波动最小幅度，米/秒
	constexpr float kPolarWindFluctuationMax = 1.5f;    // 每段风速微波动最大幅度，米/秒
	constexpr float kPolarWindVisualStartMps = 12.0f;   // 风雪带开始淡入的实数风速，米/秒
	constexpr float kPolarWindVisualFullMps = 22.0f;    // 风雪带达到完整密度的实数风速，米/秒
	constexpr float kPolarWindVisualRisePerSecond = 0.75f; // 风雪带淡入速度，每游戏秒强度
	constexpr float kPolarWindVisualFallPerSecond = 1.0f; // 风雪带淡出速度，每游戏秒强度
	constexpr float kPolarWindBreezeParticleInterval = 1.25f; // 风力预兆阶段分层粒子的最长重发间隔，游戏秒
	constexpr float kPolarWindParticleInterval = 0.72f;  // 普通强风分层粒子的重发间隔，游戏秒
	constexpr float kPolarWhiteoutParticleInterval = 0.42f; // 白毛风峰值分层粒子的重发间隔，游戏秒
	constexpr float kPolarRecoverySeconds = 3.0f;        // 假信号退出危险区的连续回落时长，游戏秒
	constexpr float kPolarFinalPreludeBuildSeconds = 0.75f; // 8-9 最终波警告内补齐三项危险的平滑爬升时长，游戏秒
	constexpr int kPolarHoleFirstColumn = 4;             // 雪穴候选第 5 列的零基索引
	constexpr int kPolarHoleLastColumn = 6;              // 雪穴候选第 7 列的零基索引

	/** 在线性端点间插值；调用方负责提供已经夹紧的极夜曲线进度。 */
	float LerpPolarValue(float earlyValue, float lateValue, float progress)
	{
		return earlyValue + (lateValue - earlyValue) * progress;
	}

	/** 只为冒险极夜关返回关内序号；无尽不继承教学或 8-9 终波脚本。 */
	int GetPolarAdventureLevelInArea(const Board& board)
	{
		return board.mIsSurvival ? 0
			: AdventureProgression::GetLevelNumberInArea(board.mLevel);
	}
}

/** 建立极夜雪原的第一轮确定性计划；非极夜地图把全部状态规范化为空。 */
void Board::InitializePolarNightEnvironment()
{
	if (!SupportsPolarNightEnvironment()) {
		mPolarNightInitialized = false;
		mPolarNightPhase = PolarNightPhase::DORMANT;
		mPolarVerticalWindDirection = VerticalWindDirection::NONE;
		mPolarFluctuationTimer = 0.0f;
		mPolarFluctuationDuration = 0.0f;
		mPolarWindParticleTimer = 0.0f;
		mPolarWindVisualStrength = 0.0f;
		mSnowHoles.fill({});
		mPendingSnowHoleSpawns.clear();
		return;
	}
	if (mPolarNightInitialized) return;
	mPolarNightInitialized = true;
	mPolarTemperatureC = kPolarBaselineTemperatureC;
	mPolarHumidityPercent = kPolarBaselineHumidity;
	mPolarWindSpeedMps = kPolarBaselineWindMps;
	mPolarLastPlanWasFalse = false;
	mPolarFirstWhiteoutCompleted = false;
	mPolarHumidityEpisodeConsumed = false;
	mPolarTutorialHoleBatchConsumed = false;
	mPolarFinalWaveUpgradeApplied = false;
	mPolarFluctuationTimer = 0.0f;
	mPolarFluctuationDuration = 0.0f;
	mPolarWindParticleTimer = 0.0f;
	mPolarWindVisualStrength = 0.0f;
	mSnowHoles.fill({});
	mPendingSnowHoleSpawns.clear();
	RollNextPolarNightPlan();
}

/** 抽取假信号危险组合；双项比单项常见，纯低温只占小比例。 */
static int RollPolarFalseDangerMask()
{
	const int roll = GameRandom::Range(1, 100);
	if (roll <= 15) return 1;
	if (roll <= 75) {
		switch (GameRandom::Range(1, 3)) {
		case 1: return 3;
		case 2: return 5;
		default: return 6;
		}
	}
	return GameRandom::Range(1, 2) == 1 ? 2 : 4;
}

/** 在指定安全侧内抽取邻近目标；若抽中的方向越界就改向，避免贴阈值停住。 */
static float RollPolarNearbyValue(float current, float lower, float upper,
	float minimumDelta, float maximumDelta)
{
	const float boundedCurrent = std::clamp(current, lower, upper);
	const float magnitude = GameRandom::Range(minimumDelta, maximumDelta);
	const float direction = GameRandom::Range(1, 2) == 1 ? -1.0f : 1.0f;
	float candidate = boundedCurrent + direction * magnitude;
	if (candidate < lower || candidate > upper) {
		candidate = boundedCurrent - direction * magnitude;
	}
	return std::clamp(candidate, lower, upper);
}

/**
 * 让保持阶段的仪表继续有缓慢变化，但每项严格留在当前计划指定的危险侧或安全侧。
 * 这里只改写已保存的曲线起点/目标；计划真假和提交累计保持独立。
 */
void Board::BeginPolarGaugeFluctuation()
{
	mPolarStartTemperatureC = mPolarTemperatureC;
	mPolarStartHumidityPercent = mPolarHumidityPercent;
	mPolarStartWindSpeedMps = mPolarWindSpeedMps;
	mPolarTargetTemperatureC = (mPolarDangerMask & 1) != 0
		? RollPolarNearbyValue(mPolarTemperatureC, -24.4f, -18.6f,
			kPolarTemperatureFluctuationMin, kPolarTemperatureFluctuationMax)
		: RollPolarNearbyValue(mPolarTemperatureC, -17.3f, -2.6f,
			kPolarTemperatureFluctuationMin, kPolarTemperatureFluctuationMax);
	mPolarTargetHumidityPercent = (mPolarDangerMask & 2) != 0
		? RollPolarNearbyValue(mPolarHumidityPercent, 86.0f, 98.5f,
			kPolarHumidityFluctuationMin, kPolarHumidityFluctuationMax)
		: RollPolarNearbyValue(mPolarHumidityPercent, 1.0f, 83.8f,
			kPolarHumidityFluctuationMin, kPolarHumidityFluctuationMax);
	mPolarTargetWindSpeedMps = (mPolarDangerMask & 4) != 0
		? RollPolarNearbyValue(mPolarWindSpeedMps, 18.8f, 28.5f,
			kPolarWindFluctuationMin, kPolarWindFluctuationMax)
		: RollPolarNearbyValue(mPolarWindSpeedMps, 0.5f, 17.2f,
			kPolarWindFluctuationMin, kPolarWindFluctuationMax);
	mPolarFluctuationTimer = 0.0f;
	mPolarFluctuationDuration = GameRandom::Range(
		kPolarFluctuationDurationMin, kPolarFluctuationDurationMax);
}

/** 当前段使用 smoothstep 连续滑动，到达目标后才锁定下一段随机目标。 */
void Board::UpdatePolarGaugeFluctuation(float deltaTime)
{
	if (mPolarFluctuationDuration <= 0.0f) BeginPolarGaugeFluctuation();
	mPolarFluctuationTimer = std::min(mPolarFluctuationDuration,
		mPolarFluctuationTimer + deltaTime);
	const float t = mPolarFluctuationDuration > 0.0f
		? mPolarFluctuationTimer / mPolarFluctuationDuration : 1.0f;
	const float eased = t * t * (3.0f - 2.0f * t);
	mPolarTemperatureC = LerpPolarValue(
		mPolarStartTemperatureC, mPolarTargetTemperatureC, eased);
	mPolarHumidityPercent = LerpPolarValue(
		mPolarStartHumidityPercent, mPolarTargetHumidityPercent, eased);
	mPolarWindSpeedMps = LerpPolarValue(
		mPolarStartWindSpeedMps, mPolarTargetWindSpeedMps, eased);
	if (mPolarFluctuationTimer >= mPolarFluctuationDuration) {
		BeginPolarGaugeFluctuation();
	}
}

/** 一次性锁定连续曲线的全部随机量，运行阶段只做插值。 */
void Board::RollNextPolarNightPlan()
{
	if (!SupportsPolarNightEnvironment()) return;
	const int levelInArea = GetPolarAdventureLevelInArea(*this);
	const bool humidityTutorial = levelInArea == 1;
	const bool windTutorial = levelInArea == 2;
	const bool forcedFirstWhiteout = levelInArea == 3 && !mPolarFirstWhiteoutCompleted;

	if (humidityTutorial || windTutorial) {
		mPolarPlanIsWhiteout = false;
		mPolarDangerMask = humidityTutorial ? 2 : 4;
	}
	else {
		mPolarPlanIsWhiteout = forcedFirstWhiteout || mPolarLastPlanWasFalse
			|| GameRandom::Range(1, 2) == 1;
		mPolarDangerMask = mPolarPlanIsWhiteout ? 7 : RollPolarFalseDangerMask();
	}
	mPolarLastPlanWasFalse = !mPolarPlanIsWhiteout;
	mPolarStartTemperatureC = mPolarTemperatureC;
	mPolarStartHumidityPercent = mPolarHumidityPercent;
	mPolarStartWindSpeedMps = mPolarWindSpeedMps;
	mPolarTargetTemperatureC = (mPolarDangerMask & 1) != 0
		? GameRandom::Range(-23.0f, -20.5f)
		: (GameRandom::Range(1, 100) <= 12
			? GameRandom::Range(-11.0f, -8.0f)
			: GameRandom::Range(-17.0f, -13.0f));
	mPolarTargetHumidityPercent = (mPolarDangerMask & 2) != 0
		? GameRandom::Range(90.0f, 96.0f) : GameRandom::Range(52.0f, 78.0f);
	mPolarTargetWindSpeedMps = (mPolarDangerMask & 4) != 0
		? GameRandom::Range(20.0f, 24.0f) : GameRandom::Range(5.0f, 14.0f);
	mPolarVerticalWindDirection = (mPolarDangerMask & 4) != 0
		? (GameRandom::Range(1, 2) == 1
			? VerticalWindDirection::UP : VerticalWindDirection::DOWN)
		: VerticalWindDirection::NONE;
	mPolarPhaseTimer = 0.0f;
	// 8-3 首轮按三项最晚越线约 16～19 秒设计，随后 5 秒提交落在约 20～25 秒。
	mPolarPhaseDuration = forcedFirstWhiteout
		? GameRandom::Range(20.0f, 24.0f) : GameRandom::Range(10.0f, 19.0f);
	mPolarNightPhase = PolarNightPhase::BUILDUP;
	mPolarAllDangerTimer = 0.0f;
	mPolarFluctuationTimer = 0.0f;
	mPolarFluctuationDuration = 0.0f;
}

/** 只在空格中均匀选择不同行；选点一旦写入行槽就不再补抽。 */
void Board::CommitSnowHoleBatch()
{
	mLastSnowHoleBatchCreated = 0;
	if (!SupportsPolarNightEnvironment()) return;
	const int levelInArea = GetPolarAdventureLevelInArea(*this);
	if (levelInArea == 1 && mPolarTutorialHoleBatchConsumed) return;

	std::vector<int> eligibleRows;
	std::array<std::vector<int>, 5> eligibleColumns;
	for (int row = 0; row < std::min(mRows, 5); ++row) {
		if (mSnowHoles[row].phase != SnowHolePhase::NONE) continue;
		for (int col = kPolarHoleFirstColumn;
			col <= std::min(kPolarHoleLastColumn, mColumns - 1); ++col) {
			Cell* cell = GetCell(row, col);
			if (cell && cell->IsEmpty() && !HasCraterAt(row, col) && !IsIceAt(row, col)) {
				eligibleColumns[row].push_back(col);
			}
		}
		if (!eligibleColumns[row].empty()) eligibleRows.push_back(row);
	}

	for (int count = 0; count < 2 && !eligibleRows.empty(); ++count) {
		const int rowIndex = GameRandom::Range(0,
			static_cast<int>(eligibleRows.size()) - 1);
		const int row = eligibleRows[rowIndex];
		const auto& columns = eligibleColumns[row];
		const int col = columns[GameRandom::Range(0,
			static_cast<int>(columns.size()) - 1)];
		mSnowHoles[row] = { col, SnowHolePhase::FORMING,
			kPolarHoleFormationSeconds };
		eligibleRows.erase(eligibleRows.begin() + rowIndex);
		++mLastSnowHoleBatchCreated;
	}
	if (levelInArea == 1) mPolarTutorialHoleBatchConsumed = true;
}

void Board::UpdateSnowHoles(float deltaTime)
{
	for (SnowHoleState& hole : mSnowHoles) {
		if (hole.phase != SnowHolePhase::FORMING) continue;
		hole.timer = std::max(0.0f, hole.timer - deltaTime);
		if (hole.timer <= 0.0f) hole.phase = SnowHolePhase::ACTIVE;
	}
}

/** 到提交边沿才读取雪穴是否仍活动；读档恢复的事务也严格走同一边沿。 */
void Board::UpdatePendingSnowHoleSpawns(float deltaTime)
{
	if (deltaTime <= 0.0f || mPendingSnowHoleSpawns.empty()) return;
	for (auto it = mPendingSnowHoleSpawns.begin();
		it != mPendingSnowHoleSpawns.end();) {
		it->timer = std::max(0.0f, it->timer - deltaTime);
		if (it->timer > 0.0f) {
			++it;
			continue;
		}

		bool useHole = it->row >= 0
			&& it->row < static_cast<int>(mSnowHoles.size())
			&& mSnowHoles[it->row].phase == SnowHolePhase::ACTIVE
			&& mSnowHoles[it->row].column == it->holeColumn;
		if (useHole && TryRejectDiscontinuousZombieEntry(it->row, it->holeColumn)) {
			useHole = false;
		}
		const float spawnX = useHole
			? GetCellCenterPosition(it->row, it->holeColumn).x
			: static_cast<float>(SCENE_WIDTH) + 40.0f;
		if (Zombie* zombie = CreateResolvedWaveZombie(it->type, it->row, spawnX)) {
			zombie->mSpawnWave = it->spawnWave;
			AssignMistFuelReward(zombie);
			if (useHole && it->tutorialSnowBurrow
				&& it->type == ZombieType::ZOMBIE_SNOW_BURROW) {
				mSnowBurrowTutorialHoleSpawnConsumed = true;
			}
		}
		it = mPendingSnowHoleSpawns.erase(it);
	}
}

/** 风雪表现从 12m/s 起连续淡入；玩法仍只在 18m/s 危险线提交垂直偏行。 */
void Board::UpdatePolarNightWindVisual(float deltaTime)
{
	const float targetStrength = mPolarVerticalWindDirection
		== VerticalWindDirection::NONE ? 0.0f : std::clamp(
			(mPolarWindSpeedMps - kPolarWindVisualStartMps)
			/ (kPolarWindVisualFullMps - kPolarWindVisualStartMps), 0.0f, 1.0f);
	const float step = deltaTime * (targetStrength > mPolarWindVisualStrength
		? kPolarWindVisualRisePerSecond : kPolarWindVisualFallPerSecond);
	if (targetStrength > mPolarWindVisualStrength) {
		mPolarWindVisualStrength = std::min(
			targetStrength, mPolarWindVisualStrength + step);
	}
	else {
		mPolarWindVisualStrength = std::max(
			targetStrength, mPolarWindVisualStrength - step);
	}

	if (!g_particleSystem || mPolarWindVisualStrength <= 0.04f
		|| mPolarVerticalWindDirection == VerticalWindDirection::NONE) {
		mPolarWindParticleTimer = 0.0f;
		return;
	}
	mPolarWindParticleTimer -= deltaTime;
	if (mPolarWindParticleTimer > 0.0f) return;

	const char* effectName = mPolarVerticalWindDirection == VerticalWindDirection::UP
		? "PolarWindUp" : "PolarWindDown";
	g_particleSystem->EmitEffect(effectName,
		Vector(165.0f, static_cast<float>(SCENE_HEIGHT) * 0.5f),
		LAYER_EFFECTS_WORLD);
	const float denseInterval = IsPolarWhiteoutCommitted()
		? kPolarWhiteoutParticleInterval : kPolarWindParticleInterval;
	mPolarWindParticleTimer = LerpPolarValue(
		kPolarWindBreezeParticleInterval, denseInterval,
		mPolarWindVisualStrength);
}

/**
 * 8-9 大波警告开始时从当前实数补齐三红。已有雪盲只延长，不重启阶段；
 * 其余计划保留连续起点，并利用完整 7.5 秒警告完成三红、提交与音画爬升。
 */
void Board::BeginPolarFinalWavePrelude()
{
	if (!SupportsPolarNightEnvironment() || mPolarFinalWaveUpgradeApplied
		|| GetPolarAdventureLevelInArea(*this) != 9) return;
	mPolarFinalWaveUpgradeApplied = true;
	if (mPolarNightPhase == PolarNightPhase::SNOW_BLIND) {
		mPolarWhiteoutTimer = std::max(
			mPolarWhiteoutTimer, kPolarFinalSnowBlindSeconds);
		return;
	}
	if (mPolarNightPhase == PolarNightPhase::WHITEOUT_RAMP) return;

	mPolarPlanIsWhiteout = true;
	mPolarDangerMask = 7;
	mPolarLastPlanWasFalse = false;
	mPolarStartTemperatureC = mPolarTemperatureC;
	mPolarStartHumidityPercent = mPolarHumidityPercent;
	mPolarStartWindSpeedMps = mPolarWindSpeedMps;
	mPolarTargetTemperatureC = -22.0f;
	mPolarTargetHumidityPercent = 93.0f;
	mPolarTargetWindSpeedMps = 22.0f;
	if (mPolarVerticalWindDirection == VerticalWindDirection::NONE) {
		mPolarVerticalWindDirection = GameRandom::Range(1, 2) == 1
			? VerticalWindDirection::UP : VerticalWindDirection::DOWN;
	}
	mPolarPhaseTimer = 0.0f;
	mPolarPhaseDuration = kPolarFinalPreludeBuildSeconds;
	mPolarNightPhase = PolarNightPhase::BUILDUP;
	mPolarAllDangerTimer = 0.0f;
	mPolarFluctuationTimer = 0.0f;
	mPolarFluctuationDuration = 0.0f;
}

/** 推进当前已锁计划，并在阈值边沿提交雪穴和不可逆白毛风。 */
void Board::UpdatePolarNightEnvironment(float deltaTime)
{
	if (!mPolarNightInitialized || deltaTime <= 0.0f) return;
	UpdateSnowHoles(deltaTime);
	UpdatePendingSnowHoleSpawns(deltaTime);

	auto setCurveValues = [this](float progress) {
		const float t = std::clamp(progress, 0.0f, 1.0f);
		const float eased = t * t * (3.0f - 2.0f * t);
		mPolarTemperatureC = LerpPolarValue(
			mPolarStartTemperatureC, mPolarTargetTemperatureC, eased);
		mPolarHumidityPercent = LerpPolarValue(
			mPolarStartHumidityPercent, mPolarTargetHumidityPercent, eased);
		mPolarWindSpeedMps = LerpPolarValue(
			mPolarStartWindSpeedMps, mPolarTargetWindSpeedMps, eased);
	};

	switch (mPolarNightPhase) {
	case PolarNightPhase::BUILDUP:
	case PolarNightPhase::RECOVERY:
		mPolarPhaseTimer = std::min(mPolarPhaseDuration,
			mPolarPhaseTimer + deltaTime);
		setCurveValues(mPolarPhaseDuration > 0.0f
			? mPolarPhaseTimer / mPolarPhaseDuration : 1.0f);
		if (mPolarPhaseTimer >= mPolarPhaseDuration) {
			if (mPolarNightPhase == PolarNightPhase::RECOVERY) {
				RollNextPolarNightPlan();
			}
			else {
				mPolarNightPhase = PolarNightPhase::DANGER_HOLD;
				mPolarPhaseTimer = 0.0f;
				mPolarPhaseDuration = mPolarPlanIsWhiteout
					? 10.0f : GameRandom::Range(12.0f, 24.0f);
				BeginPolarGaugeFluctuation();
			}
		}
		break;
	case PolarNightPhase::DANGER_HOLD:
		UpdatePolarGaugeFluctuation(deltaTime);
		mPolarPhaseTimer += deltaTime;
		if (!mPolarPlanIsWhiteout && mPolarPhaseTimer >= mPolarPhaseDuration) {
			mPolarStartTemperatureC = mPolarTemperatureC;
			mPolarStartHumidityPercent = mPolarHumidityPercent;
			mPolarStartWindSpeedMps = mPolarWindSpeedMps;
			mPolarTargetTemperatureC = kPolarBaselineTemperatureC;
			mPolarTargetHumidityPercent = kPolarBaselineHumidity;
			mPolarTargetWindSpeedMps = kPolarBaselineWindMps;
			mPolarPhaseTimer = 0.0f;
			mPolarPhaseDuration = kPolarRecoverySeconds;
			mPolarNightPhase = PolarNightPhase::RECOVERY;
			mPolarFluctuationTimer = 0.0f;
			mPolarFluctuationDuration = 0.0f;
		}
		break;
	case PolarNightPhase::WHITEOUT_RAMP:
		UpdatePolarGaugeFluctuation(deltaTime);
		mPolarPhaseTimer += deltaTime;
		if (mPolarPhaseTimer >= kPolarWhiteoutRampSeconds) {
			mPolarNightPhase = PolarNightPhase::SNOW_BLIND;
			mPolarWhiteoutTimer = GetPolarAdventureLevelInArea(*this) == 9
				? kPolarFinalSnowBlindSeconds : kPolarSnowBlindSeconds;
		}
		break;
	case PolarNightPhase::SNOW_BLIND:
		UpdatePolarGaugeFluctuation(deltaTime);
		mPolarWhiteoutTimer = std::max(0.0f, mPolarWhiteoutTimer - deltaTime);
		if (mPolarWhiteoutTimer <= 0.0f) {
			mPolarNightPhase = PolarNightPhase::FADE;
			mPolarWhiteoutTimer = kPolarWhiteoutFadeSeconds;
			mPolarStartTemperatureC = mPolarTemperatureC;
			mPolarStartHumidityPercent = mPolarHumidityPercent;
			mPolarStartWindSpeedMps = mPolarWindSpeedMps;
			mPolarTargetTemperatureC = kPolarBaselineTemperatureC;
			mPolarTargetHumidityPercent = kPolarBaselineHumidity;
			mPolarTargetWindSpeedMps = kPolarBaselineWindMps;
			mPolarPhaseTimer = 0.0f;
			mPolarPhaseDuration = kPolarPostWhiteoutRecoverySeconds;
			mPolarFluctuationTimer = 0.0f;
			mPolarFluctuationDuration = 0.0f;
			mPolarFirstWhiteoutCompleted = true;
		}
		break;
	case PolarNightPhase::FADE:
		mPolarWhiteoutTimer = std::max(0.0f, mPolarWhiteoutTimer - deltaTime);
		mPolarPhaseTimer = std::min(mPolarPhaseDuration,
			mPolarPhaseTimer + deltaTime);
		setCurveValues(mPolarPhaseDuration > 0.0f
			? mPolarPhaseTimer / mPolarPhaseDuration : 1.0f);
		if (mPolarPhaseTimer >= mPolarPhaseDuration) {
			RollNextPolarNightPlan();
		}
		break;
	case PolarNightPhase::DORMANT:
		break;
	}
	// 每帧幂等重试可覆盖旧档已完成白毛风但尚未提交教学僵尸的状态。
	TrySpawnAdaptiveHelmetTutorialWave();

	const bool highHumidity = mPolarHumidityPercent >= kPolarHumidityDanger;
	if (highHumidity) {
		mPolarHighHumidityTimer += deltaTime;
		if (!mPolarHumidityEpisodeConsumed
			&& mPolarHighHumidityTimer >= kPolarHoleCommitSeconds) {
			mPolarHumidityEpisodeConsumed = true;
			CommitSnowHoleBatch();
		}
	}
	else {
		mPolarHighHumidityTimer = 0.0f;
		mPolarHumidityEpisodeConsumed = false;
	}

	const bool allDanger = mPolarTemperatureC <= kPolarTemperatureDangerC
		&& highHumidity && mPolarWindSpeedMps >= kPolarWindDangerMps;
	if (allDanger && (mPolarNightPhase == PolarNightPhase::BUILDUP
		|| mPolarNightPhase == PolarNightPhase::DANGER_HOLD)) {
		mPolarAllDangerTimer += deltaTime;
		if (mPolarPlanIsWhiteout
			&& mPolarAllDangerTimer >= kPolarWhiteoutCommitSeconds) {
			mPolarNightPhase = PolarNightPhase::WHITEOUT_RAMP;
			mPolarPhaseTimer = 0.0f;
			mPolarWhiteoutTimer = kPolarWhiteoutRampSeconds;
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BLOVER, 0.28f);
		}
	}
	else if (!IsPolarWhiteoutCommitted()) {
		mPolarAllDangerTimer = 0.0f;
	}
	UpdatePolarNightWindVisual(deltaTime);
}

/** 雪穴只画在已锁定 Cell 中心；形成态与活动态均从首帧占用同一逻辑格。 */
void Board::DrawPolarNightGround(Graphics* g) const
{
	if (!g || !SupportsPolarNightEnvironment()) return;
	const Texture* snowHole = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_SNOW_HOLE, false);
	if (!snowHole) return;
	for (int row = 0; row < std::min(mRows, 5); ++row) {
		const SnowHoleState& hole = mSnowHoles[row];
		if (hole.phase == SnowHolePhase::NONE || hole.column < 0) continue;
		const Vector center = GetCellCenterPosition(row, hole.column)
			+ Vector(0.0f, 17.0f);
		if (hole.phase == SnowHolePhase::FORMING) {
			const float progress = std::clamp(
				1.0f - hole.timer / kPolarHoleFormationSeconds, 0.0f, 1.0f);
			const float eased = progress * progress * (3.0f - 2.0f * progress);
			const float settle = std::sin(progress * 3.14159265f)
				* progress * 0.12f;
			const float scale = 0.12f + eased * 0.88f + settle;
			const float size = 80.0f * scale;
			g->DrawTexture(snowHole, center.x - size * 0.5f,
				center.y - size * 0.5f, size, size, 0.0f,
				glm::vec4(255.0f, 255.0f, 255.0f,
					90.0f + progress * 165.0f));
		}
		else {
			g->DrawTexture(snowHole, center.x - 40.0f, center.y - 40.0f,
				80.0f, 80.0f);
		}
	}
}

int Board::GetActiveSnowHoleCount() const
{
	return static_cast<int>(std::count_if(mSnowHoles.begin(), mSnowHoles.end(),
		[](const SnowHoleState& hole) { return hole.phase != SnowHolePhase::NONE; }));
}

float Board::GetPolarWhiteoutVisualStrength() const
{
	switch (mPolarNightPhase) {
	case PolarNightPhase::WHITEOUT_RAMP:
		return std::clamp(mPolarPhaseTimer / kPolarWhiteoutRampSeconds,
			0.0f, 1.0f);
	case PolarNightPhase::SNOW_BLIND:
		return 1.0f;
	case PolarNightPhase::FADE:
		return std::clamp(mPolarWhiteoutTimer / kPolarWhiteoutFadeSeconds,
			0.0f, 1.0f);
	default:
		return 0.0f;
	}
}

bool Board::HasSnowHoleInRow(int row) const
{
	return row >= 0 && row < static_cast<int>(mSnowHoles.size())
		&& mSnowHoles[row].phase != SnowHolePhase::NONE;
}

bool Board::HasSnowHoleAt(int row, int col) const
{
	return HasSnowHoleInRow(row) && mSnowHoles[row].column == col;
}

int Board::GetSnowHoleColumn(int row) const
{
	return HasSnowHoleInRow(row) ? mSnowHoles[row].column : -1;
}

bool Board::SealSnowHole(int row)
{
	if (!HasSnowHoleInRow(row)) return false;
	mSnowHoles[row] = {};
	return true;
}

/** 锁定当前强风带来的恰好一行偏移；越界仍移动视觉落点但禁止命中。 */
bool Board::ApplyPolarLobbedWind(int sourceRow, int& landingRow, Vector& target,
	bool guided) const
{
	if (guided || mDawnNavigationTimer > 0.0f
		|| !SupportsPolarNightEnvironment() || !IsPolarWindDangerous()
		|| mPolarVerticalWindDirection == VerticalWindDirection::NONE) {
		landingRow = sourceRow;
		return true;
	}
	const int rowDelta = mPolarVerticalWindDirection == VerticalWindDirection::UP
		? -1 : 1;
	landingRow = sourceRow + rowDelta;
	target.y += static_cast<float>(rowDelta) * mCellHeight;
	return landingRow >= 0 && landingRow < mRows;
}

bool Board::PreparePolarLobbedNavigation(const Plant* plant)
{
	if (!plant || !SupportsPolarNightEnvironment()) return false;
	Plant* nearestReady = nullptr;
	int nearestDistance = 0;
	for (const int plantID : mEntityRegistry.GetAllPlantIDs()) {
		Plant* provider = mEntityRegistry.GetPlant(plantID);
		if (!provider || !provider->CoversPolarNavigationCell(
			plant->mRow, plant->mColumn)) continue;
		if (provider->IsPolarNavigationActive()) return true;
		if (!provider->IsPolarNavigationReady()) continue;
		const int distance = std::abs(provider->mRow - plant->mRow)
			+ std::abs(provider->mColumn - plant->mColumn);
		if (!nearestReady || distance < nearestDistance
			|| (distance == nearestDistance
				&& provider->mPlantID < nearestReady->mPlantID)) {
			nearestReady = provider;
			nearestDistance = distance;
		}
	}
	return nearestReady && nearestReady->ActivatePolarNavigation();
}

bool Board::SetPolarNightEnvironmentForTesting(float temperatureC,
	float humidityPercent, float windSpeedMps, VerticalWindDirection direction,
	PolarNightPhase phase, float phaseRemaining)
{
	if (!SupportsPolarNightEnvironment()) return false;
	mPolarNightInitialized = true;
	mPolarTemperatureC = std::clamp(temperatureC, -25.0f, -2.0f);
	mPolarHumidityPercent = std::clamp(humidityPercent, 0.0f, 100.0f);
	mPolarWindSpeedMps = std::clamp(windSpeedMps, 0.0f, 30.0f);
	mPolarVerticalWindDirection = direction;
	mPolarWindParticleTimer = 0.0f;
	mPolarWindVisualStrength = 0.0f;
	mPolarNightPhase = phase;
	mPolarDangerMask = (mPolarTemperatureC <= kPolarTemperatureDangerC ? 1 : 0)
		| (mPolarHumidityPercent >= kPolarHumidityDanger ? 2 : 0)
		| (mPolarWindSpeedMps >= kPolarWindDangerMps ? 4 : 0);
	mPolarPlanIsWhiteout = mPolarTemperatureC <= kPolarTemperatureDangerC
		&& mPolarHumidityPercent >= kPolarHumidityDanger
		&& mPolarWindSpeedMps >= kPolarWindDangerMps;
	mPolarStartTemperatureC = mPolarTemperatureC;
	mPolarStartHumidityPercent = mPolarHumidityPercent;
	mPolarStartWindSpeedMps = mPolarWindSpeedMps;
	mPolarTargetTemperatureC = mPolarTemperatureC;
	mPolarTargetHumidityPercent = mPolarHumidityPercent;
	mPolarTargetWindSpeedMps = mPolarWindSpeedMps;
	mPolarPhaseTimer = 0.0f;
	mPolarPhaseDuration = phase == PolarNightPhase::DANGER_HOLD ? 60.0f : 0.0f;
	mPolarAllDangerTimer = 0.0f;
	mPolarHighHumidityTimer = 0.0f;
	mPolarFluctuationTimer = 0.0f;
	mPolarFluctuationDuration = 0.0f;
	mPolarWhiteoutTimer = phase == PolarNightPhase::SNOW_BLIND
		? (phaseRemaining >= 0.0f ? std::clamp(phaseRemaining, 0.0f,
				kPolarFinalSnowBlindSeconds)
			: (GetPolarAdventureLevelInArea(*this) == 9
				? kPolarFinalSnowBlindSeconds : kPolarSnowBlindSeconds))
		: (phase == PolarNightPhase::WHITEOUT_RAMP
			? kPolarWhiteoutRampSeconds
			: (phase == PolarNightPhase::FADE ? kPolarWhiteoutFadeSeconds : 0.0f));
	if (phase == PolarNightPhase::WHITEOUT_RAMP && phaseRemaining >= 0.0f) {
		mPolarPhaseTimer = std::clamp(
			kPolarWhiteoutRampSeconds - phaseRemaining,
			0.0f, kPolarWhiteoutRampSeconds);
	}
	if (phase == PolarNightPhase::FADE) {
		mPolarTargetTemperatureC = kPolarBaselineTemperatureC;
		mPolarTargetHumidityPercent = kPolarBaselineHumidity;
		mPolarTargetWindSpeedMps = kPolarBaselineWindMps;
		mPolarPhaseDuration = kPolarPostWhiteoutRecoverySeconds;
		if (phaseRemaining >= 0.0f) {
			mPolarPhaseTimer = std::clamp(
				kPolarPostWhiteoutRecoverySeconds - phaseRemaining,
				0.0f, kPolarPostWhiteoutRecoverySeconds);
		}
	}
	else if (phase == PolarNightPhase::DANGER_HOLD
		|| phase == PolarNightPhase::WHITEOUT_RAMP
		|| phase == PolarNightPhase::SNOW_BLIND) {
		BeginPolarGaugeFluctuation();
	}
	return true;
}

bool Board::SetSnowHoleForTesting(int row, int column, SnowHolePhase phase,
	float timer)
{
	if (!SupportsPolarNightEnvironment() || row < 0
		|| row >= std::min(mRows, static_cast<int>(mSnowHoles.size()))) {
		return false;
	}
	if (phase == SnowHolePhase::NONE) {
		mSnowHoles[row] = {};
		return true;
	}
	if (column < kPolarHoleFirstColumn
		|| column > std::min(kPolarHoleLastColumn, mColumns - 1)) return false;
	mSnowHoles[row] = { column, phase, phase == SnowHolePhase::FORMING
		? std::clamp(timer, 0.0f, kPolarHoleFormationSeconds) : 0.0f };
	return true;
}

/**
 * 开发者直调若跳过最终波警告，立即提交 8-9 的最终白毛风；
 * 正常流程仍由 BeginPolarFinalWavePrelude 平滑预热。
 */
void Board::ActivatePolarFinalWaveImmediately()
{
	if (mCurrentWave != mMaxWave || !SupportsPolarNightEnvironment()
		|| GetPolarAdventureLevelInArea(*this) != 9
		|| mPolarFinalWaveUpgradeApplied) {
		return;
	}
	mPolarFinalWaveUpgradeApplied = true;
	mPolarPlanIsWhiteout = true;
	mPolarDangerMask = 7;
	mPolarTemperatureC = -22.0f;
	mPolarHumidityPercent = 93.0f;
	mPolarWindSpeedMps = 22.0f;
	if (mPolarVerticalWindDirection == VerticalWindDirection::NONE) {
		mPolarVerticalWindDirection = VerticalWindDirection::DOWN;
	}
	mPolarNightPhase = PolarNightPhase::SNOW_BLIND;
	mPolarWhiteoutTimer = kPolarFinalSnowBlindSeconds;
}

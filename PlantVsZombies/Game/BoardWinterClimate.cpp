#include "Board.h"
#include "AdventureProgression.h"
#include "./Plant/Plant.h"
#include "./Plant/PlantFootprint.h"
#include "../GameRandom.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../ParticleSystem/ParticleSystem.h"
#include "../Graphics.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
	constexpr float kWinterWarmTemperatureC = 6.0f;      // 寒潮外冬日花园的稳定环境温度（摄氏度）
	constexpr float kWinterFreezeTemperatureC = 0.0f;    // 降水转雪并开始冻结最右列的冰点（摄氏度）
	constexpr float kWinterColdTemperatureC = -12.0f;    // 寒潮最冷阶段的稳定环境温度（摄氏度）
	constexpr float kColdWaveCalmDurationMin = 35.0f;    // 两次寒潮之间温暖间隔的最短游戏秒数
	constexpr float kColdWaveCalmDurationMax = 60.0f;    // 两次寒潮之间温暖间隔的最长游戏秒数
	constexpr float kColdWaveForecastLeadTime = 20.0f;   // 寒潮降温前开始显示准确预报的游戏秒数
	constexpr float kOpeningColdWaveDelay = 12.0f;       // 7-8/7-9 开战后到固定强寒潮开始降温的游戏秒数
	constexpr float kOpeningColdWaveCoolingDuration = 15.0f; // 7-8/7-9 固定强寒潮的降温时长（游戏秒）
	constexpr float kOpeningColdWaveHoldDuration = 50.0f; // 7-8/7-9 固定强寒潮在 -12°C 的维持时长（游戏秒）
	constexpr float kOpeningColdWaveThawDuration = 30.0f; // 7-8/7-9 固定强寒潮的回暖时长（游戏秒）
	constexpr int kOpeningColdWaveFirstFrostVariant = 1; // 7-8 开幕寒潮固定使用的霜线轮廓
	constexpr int kOpeningColdWaveFinalFrostVariant = 2; // 7-9 开幕寒潮固定使用的霜线轮廓
	constexpr int kColdWaveWeakWeight = 35;              // 弱寒潮抽取权重（百分比）
	constexpr int kColdWaveNormalWeight = 45;            // 普通寒潮抽取权重（百分比）
	constexpr float kWeakColdWaveTargetC = -5.0f;        // 弱寒潮最低温基准（摄氏度）
	constexpr float kNormalColdWaveTargetC = -8.0f;      // 普通寒潮最低温基准（摄氏度）
	constexpr float kStrongColdWaveTargetC = -11.0f;     // 强寒潮最低温基准（摄氏度）
	constexpr int kColdWaveTargetJitterMin = -1;         // 同强度最低温随机偏移下限（摄氏度）
	constexpr int kColdWaveTargetJitterMax = 1;          // 同强度最低温随机偏移上限（摄氏度）
	constexpr float kWeakColdWaveCoolingMin = 21.0f;     // 弱寒潮最短降温时长（游戏秒）
	constexpr float kWeakColdWaveCoolingMax = 25.0f;     // 弱寒潮最长降温时长（游戏秒）
	constexpr float kWeakColdWaveHoldMin = 35.0f;        // 弱寒潮最短低温维持时长（游戏秒）
	constexpr float kWeakColdWaveHoldMax = 50.0f;        // 弱寒潮最长低温维持时长（游戏秒）
	constexpr float kWeakColdWaveThawMin = 24.0f;        // 弱寒潮最短回暖时长（游戏秒）
	constexpr float kWeakColdWaveThawMax = 30.0f;        // 弱寒潮最长回暖时长（游戏秒）
	constexpr float kNormalColdWaveCoolingMin = 18.0f;   // 普通寒潮最短降温时长（游戏秒）
	constexpr float kNormalColdWaveCoolingMax = 22.0f;   // 普通寒潮最长降温时长（游戏秒）
	constexpr float kNormalColdWaveHoldMin = 45.0f;      // 普通寒潮最短低温维持时长（游戏秒）
	constexpr float kNormalColdWaveHoldMax = 65.0f;      // 普通寒潮最长低温维持时长（游戏秒）
	constexpr float kNormalColdWaveThawMin = 28.0f;      // 普通寒潮最短回暖时长（游戏秒）
	constexpr float kNormalColdWaveThawMax = 36.0f;      // 普通寒潮最长回暖时长（游戏秒）
	constexpr float kStrongColdWaveCoolingMin = 15.0f;   // 强寒潮最短降温时长（游戏秒）
	constexpr float kStrongColdWaveCoolingMax = 19.0f;   // 强寒潮最长降温时长（游戏秒）
	constexpr float kStrongColdWaveHoldMin = 55.0f;      // 强寒潮最短低温维持时长（游戏秒）
	constexpr float kStrongColdWaveHoldMax = 75.0f;      // 强寒潮最长低温维持时长（游戏秒）
	constexpr float kStrongColdWaveThawMin = 34.0f;      // 强寒潮最短回暖时长（游戏秒）
	constexpr float kStrongColdWaveThawMax = 44.0f;      // 强寒潮最长回暖时长（游戏秒）
	constexpr int kWinterFrostVariantCount = 3;          // 基础冻融线与两张霜枝贴图组成的稳定轮廓数量
	constexpr float kWinterFrostFrontierAlphaScale = 1.0f; // 附加霜枝相对基础冻土透明度的亮度倍率
	constexpr int kWinterSafeColumnCount = 3;            // 温室侧永不冻结的安全列数
}

/** 建立冬日花园独立寒潮初态；其他地图保持温暖单位元。 */
void Board::InitializeWinterTemperature()
{
	if (!SupportsWinterTemperature()) {
		mWinterTemperatureInitialized = false;
		mOpeningColdWavePlanInitialized = false;
		mColdWavePhase = ColdWavePhase::CALM;
		mColdWaveTimer = 0.0f;
		mAmbientTemperatureC = kWinterWarmTemperatureC;
		mColdWaveStrength = ColdWaveStrength::STRONG;
		mColdWaveTargetTemperatureC = kWinterColdTemperatureC;
		mColdWaveCoolingDuration = 20.0f;
		mColdWaveHoldDuration = 57.5f;
		mColdWaveThawDuration = 32.0f;
		mColdWaveForecastDisrupted = false;
		mWinterFrostVariant = 0;
		return;
	}
	const bool shouldStartOpeningColdWave =
		AdventureProgression::HasOpeningColdWaveScript(mLevel)
		&& !mOpeningColdWavePlanInitialized;
	if (mWinterTemperatureInitialized && !shouldStartOpeningColdWave) return;
	mWinterTemperatureInitialized = true;
	mColdWavePhase = ColdWavePhase::CALM;
	mColdWaveForecastDisrupted = false;
	mAmbientTemperatureC = kWinterWarmTemperatureC;
	if (shouldStartOpeningColdWave) {
		// 选卡与戴夫闲聊尚未进入 GAME，不会调用这里；旧档恢复也从本次 StartGame 开始计时。
		InitializeOpeningColdWavePlan();
		mOpeningColdWavePlanInitialized = true;
		mColdWaveTimer = kOpeningColdWaveDelay;
		return;
	}
	mColdWaveTimer = GameRandom::Range(
		kColdWaveCalmDurationMin, kColdWaveCalmDurationMax);
	RollNextColdWave();
}

/**
 * 锁定 7-8/7-9 的开幕强寒潮。它不消耗随机数，确保开局预报、读档与可见霜线始终一致。
 */
void Board::InitializeOpeningColdWavePlan()
{
	mColdWaveForecastDisrupted = false;
	mColdWaveStrength = ColdWaveStrength::STRONG;
	mColdWaveTargetTemperatureC = kWinterColdTemperatureC;
	mColdWaveCoolingDuration = kOpeningColdWaveCoolingDuration;
	mColdWaveHoldDuration = kOpeningColdWaveHoldDuration;
	mColdWaveThawDuration = kOpeningColdWaveThawDuration;
	mWinterFrostVariant = mLevel == AdventureProgression::AREA_SEVEN_FINAL_LEVEL
		? kOpeningColdWaveFinalFrostVariant : kOpeningColdWaveFirstFrostVariant;
}

/**
 * 在寒潮开始前一次性抽完所有随机量，保证预报、存档和本轮冻融线外观始终一致。
 */
void Board::RollNextColdWave()
{
	mColdWaveForecastDisrupted = false;
	const int strengthRoll = GameRandom::Range(1, 100);
	if (strengthRoll <= kColdWaveWeakWeight) {
		mColdWaveStrength = ColdWaveStrength::WEAK;
	} else if (strengthRoll <= kColdWaveWeakWeight + kColdWaveNormalWeight) {
		mColdWaveStrength = ColdWaveStrength::NORMAL;
	} else {
		mColdWaveStrength = ColdWaveStrength::STRONG;
	}

	float targetBase = kNormalColdWaveTargetC;
	float coolingMin = kNormalColdWaveCoolingMin;
	float coolingMax = kNormalColdWaveCoolingMax;
	float holdMin = kNormalColdWaveHoldMin;
	float holdMax = kNormalColdWaveHoldMax;
	float thawMin = kNormalColdWaveThawMin;
	float thawMax = kNormalColdWaveThawMax;
	switch (mColdWaveStrength) {
	case ColdWaveStrength::WEAK:
		targetBase = kWeakColdWaveTargetC;
		coolingMin = kWeakColdWaveCoolingMin;
		coolingMax = kWeakColdWaveCoolingMax;
		holdMin = kWeakColdWaveHoldMin;
		holdMax = kWeakColdWaveHoldMax;
		thawMin = kWeakColdWaveThawMin;
		thawMax = kWeakColdWaveThawMax;
		break;
	case ColdWaveStrength::NORMAL:
		break;
	case ColdWaveStrength::STRONG:
		targetBase = kStrongColdWaveTargetC;
		coolingMin = kStrongColdWaveCoolingMin;
		coolingMax = kStrongColdWaveCoolingMax;
		holdMin = kStrongColdWaveHoldMin;
		holdMax = kStrongColdWaveHoldMax;
		thawMin = kStrongColdWaveThawMin;
		thawMax = kStrongColdWaveThawMax;
		break;
	}

	mColdWaveTargetTemperatureC = std::clamp(targetBase + static_cast<float>(
		GameRandom::Range(kColdWaveTargetJitterMin, kColdWaveTargetJitterMax)),
		kWinterColdTemperatureC, kWinterFreezeTemperatureC);
	mColdWaveCoolingDuration = GameRandom::Range(coolingMin, coolingMax);
	mColdWaveHoldDuration = GameRandom::Range(holdMin, holdMax);
	mColdWaveThawDuration = GameRandom::Range(thawMin, thawMax);
	mWinterFrostVariant = GameRandom::Range(0, kWinterFrostVariantCount - 1);
}

/**
 * 推进寒潮降温、低温维持与回暖；降水只在跨过冰点时重建视觉，绝不参与温度计算。
 */
void Board::UpdateWinterTemperature(float deltaTime)
{
	if (!SupportsWinterTemperature()) {
		InitializeWinterTemperature();
		return;
	}
	if (!mWinterTemperatureInitialized) InitializeWinterTemperature();
	if (deltaTime <= 0.0f) return;

	const bool wasSnowing = IsWinterPrecipitationSnow();
	mColdWaveTimer = std::max(0.0f, mColdWaveTimer - deltaTime);
	switch (mColdWavePhase) {
	case ColdWavePhase::CALM:
		mAmbientTemperatureC = kWinterWarmTemperatureC;
		if (mColdWaveTimer <= 0.0f) {
			mColdWavePhase = ColdWavePhase::COOLING;
			mColdWaveTimer = mColdWaveCoolingDuration;
		}
		break;
	case ColdWavePhase::COOLING:
	{
		const float progress = std::clamp(1.0f
			- mColdWaveTimer / mColdWaveCoolingDuration, 0.0f, 1.0f);
		mAmbientTemperatureC = kWinterWarmTemperatureC
			+ (mColdWaveTargetTemperatureC - kWinterWarmTemperatureC) * progress;
		if (mColdWaveTimer <= 0.0f) {
			mColdWavePhase = ColdWavePhase::COLD;
			mColdWaveTimer = mColdWaveHoldDuration;
			mAmbientTemperatureC = mColdWaveTargetTemperatureC;
		}
		break;
	}
	case ColdWavePhase::COLD:
		mAmbientTemperatureC = mColdWaveTargetTemperatureC;
		if (mColdWaveTimer <= 0.0f) {
			mColdWavePhase = ColdWavePhase::THAWING;
			mColdWaveTimer = mColdWaveThawDuration;
		}
		break;
	case ColdWavePhase::THAWING:
	{
		const float progress = std::clamp(1.0f
			- mColdWaveTimer / mColdWaveThawDuration, 0.0f, 1.0f);
		mAmbientTemperatureC = mColdWaveTargetTemperatureC
			+ (kWinterWarmTemperatureC - mColdWaveTargetTemperatureC) * progress;
		if (mColdWaveTimer <= 0.0f) {
			mColdWavePhase = ColdWavePhase::CALM;
			mColdWaveTimer = GameRandom::Range(
				kColdWaveCalmDurationMin, kColdWaveCalmDurationMax);
			mAmbientTemperatureC = kWinterWarmTemperatureC;
			RollNextColdWave();
		}
		break;
	}
	}

	const bool isSnowing = IsWinterPrecipitationSnow();
	if (wasSnowing == isSnowing) return;
	if (!mRainVisualEffectName.empty() && g_particleSystem) {
		g_particleSystem->StopEffect(mRainVisualEffectName);
	}
	mRainVisualActive = false;
	if (mRainIntensity != RainIntensity::CLEAR && mWeatherTimer > 0.0f) {
		EmitRainEffect(mWeatherTimer);
	}
	if (isSnowing) StopRainAudio();
	else StartRainAudio();
}


/** 提供温度计与测试共用的归一化液柱高度，避免 UI 复制寒潮温区。 */
float Board::GetWinterTemperatureGaugeRatio() const
{
	return std::clamp((mAmbientTemperatureC - kWinterColdTemperatureC)
		/ (kWinterWarmTemperatureC - kWinterColdTemperatureC), 0.0f, 1.0f);
}

float Board::GetWinterMinimumTemperatureC() const
{
	return kWinterColdTemperatureC;
}

float Board::GetWinterMaximumTemperatureC() const
{
	return kWinterWarmTemperatureC;
}

float Board::GetWinterFreezingTemperatureC() const
{
	return kWinterFreezeTemperatureC;
}

/** 寒潮预报直接读取同一个确定性阶段计时，保证揭晓结果准确且不进入雨势误报流程。 */
bool Board::HasColdWaveForecast() const
{
	return SupportsWinterTemperature() && mWinterTemperatureInitialized
		&& !mColdWaveForecastDisrupted
		&& mColdWavePhase == ColdWavePhase::CALM
		&& mColdWaveTimer > 0.0f && mColdWaveTimer <= kColdWaveForecastLeadTime;
}

bool Board::IsColdWaveForecastDisruptionVisible() const
{
	return SupportsWinterTemperature() && mWinterTemperatureInitialized
		&& mColdWaveForecastDisrupted
		&& mColdWavePhase == ColdWavePhase::CALM
		&& mColdWaveTimer > 0.0f && mColdWaveTimer <= kColdWaveForecastLeadTime;
}

/** 让一次已公开预报在本轮内失效，并按稳定植物 ID 原子清除所有依赖准备。 */
bool Board::DisruptColdWaveForecast()
{
	if (!HasColdWaveForecast()) return false;
	mColdWaveForecastDisrupted = true;
	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	for (int plantID : plantIDs) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (plant && plant->IsActive()) plant->OnColdWaveForecastDisrupted();
	}
	return true;
}

bool Board::IsColdWaveActive() const
{
	return SupportsWinterTemperature() && mWinterTemperatureInitialized
		&& mColdWavePhase != ColdWavePhase::CALM;
}

/** 温度每越过两个负温档，冻土从最右侧多推进一列；左侧三列温室区永远安全。 */
int Board::GetFrozenColumnCount() const
{
	if (!SupportsWinterTemperature()
		|| mAmbientTemperatureC > kWinterFreezeTemperatureC) return 0;
	const int maximumFrozen = std::max(0, mColumns - kWinterSafeColumnCount);
	const int coldBand = static_cast<int>(std::floor(-mAmbientTemperatureC / 2.0f)) + 1;
	return std::clamp(coldBand, 1, maximumFrozen);
}

/** 预报与实际霜线共用同一温度分档公式，但只读取本轮已经锁定的最低温。 */
int Board::GetForecastFrozenColumnCount() const
{
	if (!HasColdWaveForecast()
		|| mColdWaveTargetTemperatureC > kWinterFreezeTemperatureC) return 0;
	const int maximumFrozen = std::max(0, mColumns - kWinterSafeColumnCount);
	const int coldBand = static_cast<int>(
		std::floor(-mColdWaveTargetTemperatureC / 2.0f)) + 1;
	return std::clamp(coldBand, 1, maximumFrozen);
}

/**
 * 在相邻两档玩法冻结列之间按温度线性插值，让霜线平滑移动；整数交点仍与正式格线一致。
 */
float Board::GetWinterFrostVisualColumnCount() const
{
	if (!SupportsWinterTemperature()) return 0.0f;
	const float maximumFrozen = static_cast<float>(
		std::max(0, mColumns - kWinterSafeColumnCount));
	return std::clamp(1.0f - mAmbientTemperatureC / 2.0f,
		0.0f, maximumFrozen);
}

int Board::GetFirstFrozenColumn() const
{
	return mColumns - GetFrozenColumnCount();
}

bool Board::IsCellFrozen(int row, int col) const
{
	return row >= 0 && row < mRows && col >= 0 && col < mColumns
		&& GetFrozenColumnCount() > 0 && col >= GetFirstFrozenColumn();
}


bool Board::IsCellInColdWaveForecast(int row, int col) const
{
	const int frozenColumns = GetForecastFrozenColumnCount();
	return row >= 0 && row < mRows && col >= 0 && col < mColumns
		&& frozenColumns > 0 && col >= mColumns - frozenColumns;
}

bool Board::IsPlantFootprintFrozen(PlantType type, int row, int anchorColumn) const
{
	if (!SupportsWinterTemperature()) return false;
	const PlantFootprint footprint = GetPlantFootprint(type);
	for (std::size_t i = 0; i < footprint.count; ++i) {
		if (IsCellFrozen(row + footprint.cells[i].rowOffset,
			anchorColumn + footprint.cells[i].columnOffset)) return true;
	}
	return false;
}

bool Board::IsWinterPrecipitationSnow() const
{
	return SupportsWinterTemperature()
		&& mAmbientTemperatureC <= kWinterFreezeTemperatureC
		&& mRainIntensity != RainIntensity::CLEAR;
}

bool Board::SetWinterTemperatureForTesting(float temperatureC,
	ColdWavePhase phase, float remaining, ColdWaveStrength strength,
	float targetTemperatureC, float coolingDuration, float holdDuration,
	float thawDuration, int frostVariant)
{
	if (!SupportsWinterTemperature() || !std::isfinite(temperatureC)
		|| !std::isfinite(remaining) || !std::isfinite(targetTemperatureC)
		|| !std::isfinite(coolingDuration) || coolingDuration <= 0.0f
		|| !std::isfinite(holdDuration) || holdDuration <= 0.0f
		|| !std::isfinite(thawDuration) || thawDuration <= 0.0f
		|| phase < ColdWavePhase::CALM || phase > ColdWavePhase::THAWING) {
		return false;
	}
	const bool wasSnowing = IsWinterPrecipitationSnow();
	mWinterTemperatureInitialized = true;
	mColdWaveForecastDisrupted = false;
	mColdWavePhase = phase;
	mColdWaveTimer = std::max(0.0f, remaining);
	mColdWaveStrength = std::clamp(strength,
		ColdWaveStrength::WEAK, ColdWaveStrength::STRONG);
	mColdWaveTargetTemperatureC = std::clamp(targetTemperatureC,
		kWinterColdTemperatureC, kWinterFreezeTemperatureC);
	mColdWaveCoolingDuration = coolingDuration;
	mColdWaveHoldDuration = holdDuration;
	mColdWaveThawDuration = thawDuration;
	mWinterFrostVariant = std::clamp(frostVariant, 0, kWinterFrostVariantCount - 1);
	mAmbientTemperatureC = std::clamp(temperatureC,
		kWinterColdTemperatureC, kWinterWarmTemperatureC);
	StopTyphoon();
	const bool isSnowing = IsWinterPrecipitationSnow();
	if (wasSnowing != isSnowing) {
		if (!mRainVisualEffectName.empty() && g_particleSystem) {
			g_particleSystem->StopEffect(mRainVisualEffectName);
		}
		mRainVisualActive = false;
		if (mRainIntensity != RainIntensity::CLEAR && mWeatherTimer > 0.0f) {
			EmitRainEffect(mWeatherTimer);
		}
		if (isSnowing) StopRainAudio();
		else StartRainAudio();
	}
	return true;
}


/** 绘制从僵尸侧向温室推进的连续霜雪纹理，并按本轮锁定变体叠加不规则霜枝。 */
void Board::DrawWinterFrost(Graphics* g) const
{
	if (!g || !SupportsWinterTemperature()) return;
	const float visualColumns = GetWinterFrostVisualColumnCount();
	if (visualColumns <= 0.0f) return;
	const Texture* frost = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_WINTER_FROST_OVERLAY_V2, false);
	if (!frost) return;

	constexpr float kFrostFrontierBleed = 24.0f; // 不规则霜枝越过逻辑冻融线的最大视觉宽度，单位像素
	const float boundaryX = CELL_INITALIZE_POS_X
		+ (static_cast<float>(mColumns) - visualColumns) * CELL_COLLIDER_SIZE_X;
	const float left = boundaryX - kFrostFrontierBleed;
	const float width = visualColumns * CELL_COLLIDER_SIZE_X
		+ kFrostFrontierBleed;
	const float height = static_cast<float>(mRows) * mCellHeight;
	const float maximumFrozen = static_cast<float>(
		std::max(1, mColumns - kWinterSafeColumnCount));
	const float alpha = visualColumns <= 1.0f
		? 75.0f * visualColumns
		: 75.0f + 170.0f * std::clamp(
			(visualColumns - 1.0f) / (maximumFrozen - 1.0f), 0.0f, 1.0f);
	const glm::vec4 tint(255.0f, 255.0f, 255.0f, alpha);
	const BlendMode previousBlend = g->GetBlendMode();
	g->SetBlendMode(BlendMode::Alpha);
	g->DrawTexture(frost, left, mCellInitialY, width, height, 0.0f, tint);

	// 变体贴图使用纯黑底加色混合：黑色不覆盖草坪，只把左侧冻融前缘的霜枝提亮。
	if (mWinterFrostVariant > 0) {
		const std::string& frontierKey = mWinterFrostVariant == 1
			? ResourceKeys::Textures::IMAGE_WINTER_FROST_FRONTIER_VARIANT_1
			: ResourceKeys::Textures::IMAGE_WINTER_FROST_FRONTIER_VARIANT_2;
		const Texture* frontier = ResourceManager::GetInstance().GetTexture(
			frontierKey, false);
		if (frontier) {
			g->SetBlendMode(BlendMode::Add);
			g->DrawTexture(frontier, left, mCellInitialY, width, height, 0.0f,
				glm::vec4(255.0f, 255.0f, 255.0f,
					alpha * kWinterFrostFrontierAlphaScale));
		}
	}
	g->SetBlendMode(previousBlend);
}

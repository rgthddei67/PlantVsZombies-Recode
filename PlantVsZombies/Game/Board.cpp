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
#include <array>
#include <algorithm>   // std::max, std::swap
#include <cmath>       // std::lround

namespace {
	constexpr float kPoolCellInitialY = 85.0f;            // 泳池背景共用的六行网格首行顶部世界坐标（像素）
	constexpr float kPoolCellHeight = 85.0f;              // 泳池六行的逻辑格高（像素）；列宽仍保持 80
	constexpr float kZombieSpawnBaseOffsetY = 2.0f;       // 第一、二大关已确认正确的僵尸行中心统一基线（像素）
	constexpr float kPoolBackgroundZombieSpawnYOffset = 0.0f; // 所有泳池背景、所有行共用的僵尸额外基线，单位：像素
	constexpr float kThirdAreaZombieAlignmentOffsetY = 10.0f; // 仅第三大关所有行使用的地图对齐基线，单位：像素
	constexpr float kPoolRowZombieSpawnYOffset = 30.0f;   // 仅水路行僵尸的画面下沉量；碰撞基线不含此值，单位：像素
	constexpr int kPoolFirstRow = 2;                      // 泳池第一条水路的 0-based 行号
	constexpr int kPoolLastRow = 3;                       // 泳池最后一条水路的 0-based 行号
	constexpr int kPoolFirstWaterSpawnWave = 5;           // 泳池自然波次从第几波起允许选择水路
	constexpr float kFirstRainDelayMin = 90.0f;          // 开局到首场雨的最短等待时间（秒）
	constexpr float kFirstRainDelayMax = 105.0f;          // 开局到首场雨的最长等待时间（秒）
	constexpr float kClearWeatherDelayMin = 15.0f;       // 两场雨之间的最短晴空间隔（秒）
	constexpr float kClearWeatherDelayMax = 40.0f;       // 两场雨之间的最长晴空间隔（秒）
	constexpr float kRainDurationMin = 85.0f;            // 一场新雨第一个雨段的最短持续时间（秒）
	constexpr float kRainDurationMax = 150.0f;           // 一场新雨第一个雨段的最长持续时间（秒）
	constexpr float kRainTailDurationMin = 45.0f;        // 雨势切档后尾雨段的最短持续时间（秒）
	constexpr float kRainTailDurationMax = 80.0f;        // 雨势切档后尾雨段的最长持续时间（秒）
	constexpr float kLateLightRainDurationMin = 45.0f;   // 满压力下新小雨最短持续时间（秒），加快低威胁天气周转
	constexpr float kLateLightRainDurationMax = 70.0f;   // 满压力下新小雨最长持续时间（秒）
	constexpr float kLateMediumRainDurationMin = 60.0f;  // 满压力下新中雨最短持续时间（秒）
	constexpr float kLateMediumRainDurationMax = 95.0f;  // 满压力下新中雨最长持续时间（秒）
	constexpr float kLateLightRainTailMin = 20.0f;       // 满压力下尾段小雨最短持续时间（秒）
	constexpr float kLateLightRainTailMax = 35.0f;       // 满压力下尾段小雨最长持续时间（秒）
	constexpr float kLateMediumRainTailMin = 30.0f;      // 满压力下尾段中雨最短持续时间（秒）
	constexpr float kLateMediumRainTailMax = 50.0f;      // 满压力下尾段中雨最长持续时间（秒）
	constexpr float kWeatherForecastLeadTime = 15.0f;    // 阶段结束前多少秒预抽取并展示下一天气
	constexpr float kHeavyRainPromptLeadTime = 5.0f;     // 真正进入新大雨前弹出分级文字警报的提前量（游戏秒）
	constexpr int kWeatherForecastAccuracyPercent = 75;  // 前期天气预警准确率（百分比）
	constexpr int kLateWeatherForecastAccuracyPercent = 95; // 满压力天气预警准确率上限（百分比）
	constexpr int kEliteDancerMutationChancePercent = 50; // 台风以上普通舞王变异为精英舞王的概率（百分比）
	constexpr int kEliteDancerMaxPerWave = 2;             // 每波最多允许生成的精英舞王数量；超额候选直接跳过
	constexpr int kReinforcedDoorMaxPerWave = 2;          // 每波最多正式生成的加固铁门数量；超额候选直接跳过
	constexpr int kElitePolevaulterMaxPerWave = 2;        // 每波最多正式生成的精英撑杆数量；超额候选直接跳过
	constexpr int kEliteScaredyShroomPlantLimit = 3;      // 每个关卡累计最多种植的精英胆小菇数量
	constexpr int kWaveCandidateAttemptLimit = MAX_ZOMBIES_PER_WAVE * 10; // 单波候选尝试上限，防止仅剩受限类型时死循环
	constexpr float kWeatherTransitionDuration = 2.0f;   // 雨势切换时倍率、暗幕与雨声音量的平滑过渡时长（游戏秒）
	constexpr float kLateWeatherRampStart = 0.40f;       // 普通关波次进度超过该比例后开始增强后期天气（0～1）
	constexpr float kAdventurePressureFullProgress = 0.7f; // 普通关到该波次进度时达到完整天气压力（0～1）
	constexpr int kSurvivalLateWeatherFullRound = 8;     // 黑夜无尽到该轮起按完整后期天气权重计算
	constexpr int kSurvivalPressureStartRound = 8;       // 黑夜无尽从该轮起在基础雨势之上继续增加天气压力
	constexpr int kSurvivalPressureFullRound = 20;       // 黑夜无尽到该轮达到完整天气压力，之后不再继续放大
	constexpr int kLightRainWeight = 45;                 // 小雨相对权重；数值越大越容易抽中
	constexpr int kMediumRainWeight = 40;                // 中雨相对权重；数值越大越容易抽中
	constexpr int kHeavyRainWeight = 40;                 // 大雨相对权重；数值越大越容易抽中
	constexpr int kLateLightRainWeight = 10;             // 满压力小雨目标权重，保留少量天气层次
	constexpr int kLateMediumRainWeight = 25;            // 满压力中雨目标权重，避免长时间低威胁天气
	constexpr int kLateHeavyRainWeight = 65;             // 满压力大雨目标权重，不含弱天气保底
	constexpr int kClearHoldWeight = 15;                 // 前期晴天阶段结束后继续晴天的相对权重
	constexpr int kLateClearHoldWeight = 0;              // 满压力不连续续晴；每场雨后的晴空间隔仍保留
	constexpr float kWeakWeatherPityStart = 0.75f;       // 天气导演达到该强度后，一次弱天气便触发下轮大雨保底
	constexpr int kWeakWeatherPityMax = 1;               // 连续弱天气计数上限，保证后期最多间隔一轮弱天气
	constexpr int kLightToMediumWeight = 30;             // 初始小雨结束时增强为中雨的相对权重
	constexpr int kLightToHeavyWeight = 45;              // 初始小雨结束时骤增为大雨的相对权重
	constexpr int kLightToClearWeight = 30;              // 初始小雨结束时直接放晴的相对权重
	constexpr int kLightTransitionWeightTotal = kLightToMediumWeight + kLightToHeavyWeight + kLightToClearWeight; // 初始小雨转档总权重
	constexpr int kMediumToLightWeight = 20;             // 中雨结束时衰减为小雨的相对权重
	constexpr int kMediumToClearWeight = 70;             // 中雨结束时直接放晴的相对权重
	constexpr int kLateMediumToLightWeight = 15;         // 后期中雨衰减为小雨的目标权重，减少乏味的小雨尾段
	constexpr int kLateMediumToClearWeight = 95;         // 后期中雨直接放晴的目标权重
	constexpr int kMediumHoldWeight = 15;                // 每场中雨至多一次同档续期的相对权重
	constexpr int kHeavyToMediumWeight = 45;             // 大雨结束时衰减为中雨的相对权重
	constexpr int kHeavyToLightWeight = 20;              // 大雨结束时直接衰减为小雨的相对权重
	constexpr int kHeavyToClearWeight = 40;              // 大雨结束时直接放晴的相对权重
	constexpr int kLateHeavyToMediumWeight = 55;         // 后期大雨衰减为中雨的目标权重
	constexpr int kLateHeavyToLightWeight = 5;           // 后期大雨直接衰减为小雨的目标权重
	constexpr int kLateHeavyToClearWeight = 40;          // 后期大雨直接放晴的目标权重
	constexpr int kHeavyHoldWeight = 20;                 // 每场大雨至多一次同档续期的相对权重
	constexpr float kLightZombieSpeed = 1.15f;           // 小雨僵尸动作与移动速度倍率
	constexpr float kMediumZombieSpeed = 1.25f;          // 中雨僵尸动作与移动速度倍率
	constexpr float kHeavyZombieSpeed = 1.40f;           // 大雨僵尸动作与移动速度倍率
	constexpr float kPressuredLightZombieSpeed = 1.20f;  // 完整后期压力下小雨僵尸动作与移动速度倍率
	constexpr float kPressuredMediumZombieSpeed = 1.35f; // 完整后期压力下中雨僵尸动作与移动速度倍率
	constexpr float kPressuredHeavyZombieSpeed = 1.55f;  // 完整后期压力下大雨僵尸动作与移动速度倍率
	constexpr float kLightPlantActionSpeed = 1.10f;      // 小雨植物攻击、生产、成长与恢复速度倍率
	constexpr float kMediumPlantActionSpeed = 1.15f;     // 中雨植物攻击、生产、成长与恢复速度倍率
	constexpr float kHeavyPlantActionSpeed = 1.20f;      // 大雨植物攻击、生产、成长与恢复速度倍率
	constexpr float kPressuredLightPlantActionSpeed = 1.00f;  // 满压力小雨植物行动倍率，收回前期增益但不惩罚
	constexpr float kPressuredMediumPlantActionSpeed = 0.97f; // 满压力中雨植物行动倍率，提供温和输出压制
	constexpr float kPressuredHeavyPlantActionSpeed = 0.93f;  // 满压力大雨植物行动倍率，避免成熟阵容无视天气
	constexpr float kLightOverlayAlpha = 34.0f;          // 小雨世界蓝灰暗幕透明度（0～255）
	constexpr float kMediumOverlayAlpha = 55.0f;         // 中雨世界蓝灰暗幕透明度（0～255）
	constexpr float kHeavyOverlayAlpha = 110.0f;          // 大雨世界蓝灰暗幕透明度（0～255）
	constexpr float kLightRainVolume = 0.20f;            // 小雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kMediumRainVolume = 0.28f;           // 中雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kHeavyRainVolume = 0.48f;            // 大雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kLightSplashDelayMin = 0.16f;         // 小雨两次地面水花的最短间隔（秒，约每秒 5 次）
	constexpr float kLightSplashDelayMax = 0.24f;         // 小雨两次地面水花的最长间隔（秒，约每秒 5 次）
	constexpr float kMediumSplashDelayMin = 0.08f;        // 中雨两次地面水花的最短间隔（秒，约每秒 10 次）
	constexpr float kMediumSplashDelayMax = 0.12f;        // 中雨两次地面水花的最长间隔（秒，约每秒 10 次）
	constexpr float kHeavySplashDelayMin = 0.01f;         // 大雨两次地面水花的最短间隔（秒，约每秒 20多 次）
	constexpr float kHeavySplashDelayMax = 0.03f;         // 大雨两次地面水花的最长间隔（秒，约每秒 20多 次）
	constexpr float kRainSplashEdgePadding = 18.0f;       // 水花中心距草地网格边缘的安全距离（像素）
	constexpr float kLightningDelayMin = 3.5f;           // 大雨开始后首次闪电的最短等待时间（秒）
	constexpr float kLightningDelayMax = 7.0f;           // 大雨开始后首次闪电的最长等待时间（秒）
	constexpr float kLightningRepeatMin = 5.0f;          // 大雨中两次闪电的最短间隔（秒）
	constexpr float kLightningRepeatMax = 10.0f;         // 大雨中两次闪电的最长间隔（秒）
	constexpr float kLightningFlashDuration = 0.18f;     // 单次闪电白闪持续时间（秒）
	constexpr float kLightningFlashPeakAlpha = 105.0f;   // 单次闪电白闪峰值透明度（0～255）
	constexpr int kTyphoonChanceEarlyPercent = 75;       // 新大雨阶段附加台风的前期基础概率（百分比）
	constexpr int kTyphoonChanceLatePercent = 95;        // 满压力新大雨附加台风的基础概率（百分比）
	constexpr int kTyphoonPityPerMissPercent = 20;       // 每连续落空一个新大雨阶段，下次台风概率增加的百分点
	constexpr int kTyphoonChanceMaxPercent = 100;        // 满压力大雨落空一次后，下次台风必定命中
	constexpr int kTyphoonPityMaxMisses = 4;             // 记入概率计算的最大连续落空次数，避免损坏存档放大整数
	constexpr int kTyphoonWeight = 40;                   // 命中台风后普通台风的相对权重
	constexpr int kSevereTyphoonWeight = 45;             // 命中台风后强台风的相对权重
	constexpr int kSuperTyphoonWeight = 45;              // 命中台风后超强台风的相对权重
	constexpr int kLateTyphoonWeight = 15;               // 满压力普通台风目标权重，仍保留少量温和结果
	constexpr int kLateSevereTyphoonWeight = 45;         // 满压力强台风目标权重
	constexpr int kLateSuperTyphoonWeight = 40;          // 满压力超强台风目标权重，明显增压但仍不过半
	constexpr float kWindDirectionDurationMin = 15.0f;   // 同一风向至少维持的游戏秒数
	constexpr float kWindDirectionDurationMax = 25.0f;   // 同一风向至多维持的游戏秒数
	constexpr float kSuperTyphoonDecayMin = 55.0f;       // 超强台风衰减为强台风前的最短持续时间（游戏秒）
	constexpr float kSuperTyphoonDecayMax = 70.0f;       // 超强台风衰减为强台风前的最长持续时间（游戏秒）
	constexpr float kSevereTyphoonDecayMin = 50.0f;      // 强台风衰减为普通台风前的最短持续时间（游戏秒）
	constexpr float kSevereTyphoonDecayMax = 65.0f;      // 强台风衰减为普通台风前的最长持续时间（游戏秒）
	constexpr float kTyphoonDecayMin = 50.0f;            // 普通台风完全消散前的最短持续时间（游戏秒）
	constexpr float kTyphoonDecayMax = 55.0f;            // 普通台风完全消散前的最长持续时间（游戏秒）
	constexpr float kTyphoonWindParticleInterval = 0.30f; // 普通台风风线发射间隔；越大视觉浓度越低（游戏秒）
	constexpr float kSevereWindParticleInterval = 0.20f; // 强台风风线发射间隔；用浓度表达强度（游戏秒）
	constexpr float kSuperWindParticleInterval = 0.10f;  // 超强台风风线发射间隔；三档中视觉浓度最高（游戏秒）
	constexpr float kWindParticleOriginPadding = 40.0f;  // 风线从逻辑画面左右边缘外生成的距离（像素）
	constexpr float kTyphoonGustWarningTime = 5.0f;      // 阵风前常驻实况进入警示色的秒数
	constexpr float kTyphoonPlantSlideDuration = 0.45f;  // 植物逻辑换格后画面平滑追赶的游戏秒数
	constexpr float kSevereGustDuration = 1.80f;         // 强台风单次阵风持续时间（游戏秒）
	constexpr float kSuperGustDuration = 2.50f;          // 超强台风单次阵风持续时间（游戏秒）
	constexpr float kGustPlantMoveProgressMin = 0.25f;   // 植物最早在阵风进度 25% 时结算，避免开始瞬移
	constexpr float kGustPlantMoveProgressMax = 0.75f;   // 植物最晚在阵风进度 75% 时结算，保留受力过程
	constexpr float kSevereGustZombiePeakSpeed = 80.0f;  // 强台风阵风吹动僵尸的峰值速度（像素/游戏秒）
	constexpr float kSuperGustZombiePeakSpeed = 120.0f;   // 超强台风阵风吹动僵尸的峰值速度（像素/游戏秒）
	constexpr float kGustZombieFrontLimitPadding = 60.0f; // 僵尸被吹向前线时距画面右缘的最大余量（像素）
	constexpr float kTyphoonGustIntervalMin = 15.0f;     // 普通台风阵风最短间隔（游戏秒）
	constexpr float kTyphoonGustIntervalMax = 30.0f;     // 普通台风阵风最长间隔（游戏秒）
	constexpr float kSevereGustIntervalMin = 8.0f;      // 强台风阵风最短间隔（游戏秒），阶段内通常触发 1～2 次
	constexpr float kSevereGustIntervalMax = 13.0f;      // 强台风阵风最长间隔；短于最短衰减时间以保证至少一次
	constexpr float kSuperGustIntervalMin = 9.0f;       // 超强台风阵风最短间隔；位移更远，频率略低于强台风
	constexpr float kSuperGustIntervalMax = 14.0f;       // 超强台风阵风最长间隔；短于最短衰减时间以保证至少一次
	constexpr int kTyphoonGustDistance = 0;              // 普通台风吹不动植物，仅保留顺逆风僵尸倍率
	constexpr int kSevereGustDistance = 1;               // 强台风每次吹动的整数格数
	constexpr int kSuperGustDistance = 2;                // 超强台风每次吹动的整数格数
	constexpr int kTyphoonMaxGusts = 0;                  // 普通台风不触发植物位移阵风，降低首次遇见的压迫感
	constexpr int kSevereMaxGusts = 3;                   // 强台风单个大雨阶段最多阵风次数
	constexpr int kSuperMaxGusts = 3;                    // 超强台风单个大雨阶段最多阵风次数
	constexpr float kTyphoonTailwindZombieMove = 1.10f;  // 普通台风顺风僵尸水平移动倍率（相对当前雨天）
	constexpr float kTyphoonHeadwindZombieMove = 0.90f;  // 普通台风逆风僵尸水平移动倍率（相对当前雨天）
	constexpr float kSevereTailwindZombieMove = 1.20f;   // 强台风顺风僵尸水平移动倍率（相对当前雨天）
	constexpr float kSevereHeadwindZombieMove = 0.80f;   // 强台风逆风僵尸水平移动倍率（相对当前雨天）
	constexpr float kSuperTailwindZombieMove = 1.30f;    // 超强台风顺风僵尸水平移动倍率（相对当前雨天）
	constexpr float kSuperHeadwindZombieMove = 0.70f;    // 超强台风逆风僵尸水平移动倍率（相对当前雨天）
	constexpr float kTyphoonTailwindBulletSpeed = 1.15f; // 普通台风顺风轻型植物子弹水平速度倍率
	constexpr float kTyphoonHeadwindBulletSpeed = 0.85f; // 普通台风逆风轻型植物子弹水平速度倍率
	constexpr float kSevereTailwindBulletSpeed = 1.25f;  // 强台风顺风轻型植物子弹水平速度倍率
	constexpr float kSevereHeadwindBulletSpeed = 0.75f;  // 强台风逆风轻型植物子弹水平速度倍率
	constexpr float kSuperTailwindBulletSpeed = 1.45f;   // 超强台风顺风轻型植物子弹水平速度倍率
	constexpr float kSuperHeadwindBulletSpeed = 0.55f;   // 超强台风逆风轻型植物子弹水平速度倍率
	constexpr float kTyphoonTailwindBulletDamage = 1.10f; // 普通台风顺风轻型植物子弹命中伤害倍率
	constexpr float kTyphoonHeadwindBulletDamage = 0.90f; // 普通台风逆风轻型植物子弹命中伤害倍率
	constexpr float kSevereTailwindBulletDamage = 1.15f;  // 强台风顺风轻型植物子弹命中伤害倍率
	constexpr float kSevereHeadwindBulletDamage = 0.85f;  // 强台风逆风轻型植物子弹命中伤害倍率
	constexpr float kSuperTailwindBulletDamage = 1.20f;   // 超强台风顺风轻型植物子弹命中伤害倍率
	constexpr float kSuperHeadwindBulletDamage = 0.80f;   // 超强台风逆风轻型植物子弹命中伤害倍率

	/** 返回雨势与实时风向共同决定的雨丝特效名；台风只存在于大雨。 */
	const char* RainEffectName(RainIntensity intensity, WindDirection direction)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:  return "RainLight";
		case RainIntensity::MEDIUM: return "RainMedium";
		case RainIntensity::HEAVY:
			if (direction == WindDirection::TOWARD_HOUSE) return "RainHeavyTowardHouse";
			if (direction == WindDirection::TOWARD_FRONT) return "RainHeavyTowardFront";
			return "RainHeavy";
		case RainIntensity::CLEAR:  break;
		}
		return "";
	}

	/** 返回实时吹向对应的横向风线粒子名；名字必须匹配 XML 首个 Emitter。 */
	const char* WindEffectName(WindDirection direction)
	{
		switch (direction) {
		case WindDirection::TOWARD_HOUSE: return "WindTowardHouse";
		case WindDirection::TOWARD_FRONT: return "WindTowardFront";
		case WindDirection::NONE:         return "";
		}
		return "";
	}

	/** 按独立二选一结果返回风实际吹向；固定点数仅供 AutoTest 覆盖同向与换向分支。 */
	WindDirection WindDirectionForRoll(int directionRoll)
	{
		const int roll = directionRoll > 0 ? directionRoll : GameRandom::Range(1, 2);
		return roll == 1 ? WindDirection::TOWARD_HOUSE : WindDirection::TOWARD_FRONT;
	}

	/** 在两档调参权重间插值并四舍五入，保证后期变化连续而非跨波次突跳。 */
	int LerpWeatherWeight(int earlyWeight, int lateWeight, float lateFactor)
	{
		return static_cast<int>(std::lround(static_cast<float>(earlyWeight)
			+ static_cast<float>(lateWeight - earlyWeight) * lateFactor));
	}

	/** 在线性调参端点间插值；调用方负责提供已经夹紧的天气导演强度。 */
	float LerpWeatherValue(float earlyValue, float lateValue, float directorFactor)
	{
		return earlyValue + (lateValue - earlyValue) * directorFactor;
	}

	struct NewWeatherWeights {
		int clear = 0;
		int light = 0;
		int medium = 0;
		int heavy = 0;

		int Total() const { return clear + light + medium + heavy; }
	};

	/** 返回新天气前沿的动态权重；满压力仍保留小/中雨，但不再连续续晴。 */
	NewWeatherWeights BuildNewWeatherWeights(float directorFactor)
	{
		return {
			LerpWeatherWeight(kClearHoldWeight, kLateClearHoldWeight, directorFactor),
			LerpWeatherWeight(kLightRainWeight, kLateLightRainWeight, directorFactor),
			LerpWeatherWeight(kMediumRainWeight, kLateMediumRainWeight, directorFactor),
			LerpWeatherWeight(kHeavyRainWeight, kLateHeavyRainWeight, directorFactor),
		};
	}

	struct TyphoonWeights {
		int normal = 0;
		int severe = 0;
		int super = 0;

		int Total() const { return normal + severe + super; }
	};

	/** 返回台风强度的动态权重；满压力强/超强合计 85%，避免只见无位移普通风。 */
	TyphoonWeights BuildTyphoonWeights(float directorFactor)
	{
		return {
			LerpWeatherWeight(kTyphoonWeight, kLateTyphoonWeight, directorFactor),
			LerpWeatherWeight(kSevereTyphoonWeight, kLateSevereTyphoonWeight, directorFactor),
			LerpWeatherWeight(kSuperTyphoonWeight, kLateSuperTyphoonWeight, directorFactor),
		};
	}

	/** 按雨势与导演强度缩短低威胁新雨；大雨保持原持续时间。 */
	float RandomNewRainDuration(RainIntensity intensity, float directorFactor)
	{
		float minimum = kRainDurationMin;
		float maximum = kRainDurationMax;
		if (intensity == RainIntensity::LIGHT) {
			minimum = LerpWeatherValue(kRainDurationMin, kLateLightRainDurationMin, directorFactor);
			maximum = LerpWeatherValue(kRainDurationMax, kLateLightRainDurationMax, directorFactor);
		}
		else if (intensity == RainIntensity::MEDIUM) {
			minimum = LerpWeatherValue(kRainDurationMin, kLateMediumRainDurationMin, directorFactor);
			maximum = LerpWeatherValue(kRainDurationMax, kLateMediumRainDurationMax, directorFactor);
		}
		return GameRandom::Range(minimum, maximum);
	}

	/** 按雨势与导演强度缩短低威胁尾雨；大雨续期仍使用原区间。 */
	float RandomTailRainDuration(RainIntensity intensity, float directorFactor)
	{
		float minimum = kRainTailDurationMin;
		float maximum = kRainTailDurationMax;
		if (intensity == RainIntensity::LIGHT) {
			minimum = LerpWeatherValue(kRainTailDurationMin, kLateLightRainTailMin, directorFactor);
			maximum = LerpWeatherValue(kRainTailDurationMax, kLateLightRainTailMax, directorFactor);
		}
		else if (intensity == RainIntensity::MEDIUM) {
			minimum = LerpWeatherValue(kRainTailDurationMin, kLateMediumRainTailMin, directorFactor);
			maximum = LerpWeatherValue(kRainTailDurationMax, kLateMediumRainTailMax, directorFactor);
		}
		return GameRandom::Range(minimum, maximum);
	}

	/** 返回台风强度对应的单次阵风位移格数。 */
	int TyphoonGustDistance(TyphoonStrength strength)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON: return kTyphoonGustDistance;
		case TyphoonStrength::SEVERE:  return kSevereGustDistance;
		case TyphoonStrength::SUPER:   return kSuperGustDistance;
		case TyphoonStrength::NONE:    return 0;
		}
		return 0;
	}

	/** 返回台风强度对应的阶段阵风次数上限。 */
	int TyphoonMaxGusts(TyphoonStrength strength)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON: return kTyphoonMaxGusts;
		case TyphoonStrength::SEVERE:  return kSevereMaxGusts;
		case TyphoonStrength::SUPER:   return kSuperMaxGusts;
		case TyphoonStrength::NONE:    return 0;
		}
		return 0;
	}

	/** 返回强度对应的一次阵风持续时间；普通台风没有可产生位移的阵风。 */
	float TyphoonGustDuration(TyphoonStrength strength)
	{
		switch (strength) {
		case TyphoonStrength::SEVERE:  return kSevereGustDuration;
		case TyphoonStrength::SUPER:   return kSuperGustDuration;
		case TyphoonStrength::TYPHOON:
		case TyphoonStrength::NONE:    return 0.0f;
		}
		return 0.0f;
	}

	/** 返回阵风吹动僵尸的峰值速度；实际速度还会乘平滑起落包络。 */
	float TyphoonGustZombiePeakSpeed(TyphoonStrength strength)
	{
		switch (strength) {
		case TyphoonStrength::SEVERE:  return kSevereGustZombiePeakSpeed;
		case TyphoonStrength::SUPER:   return kSuperGustZombiePeakSpeed;
		case TyphoonStrength::TYPHOON:
		case TyphoonStrength::NONE:    return 0.0f;
		}
		return 0.0f;
	}

	/** 按台风强度抽取下一次阵风间隔。 */
	float RandomTyphoonGustInterval(TyphoonStrength strength)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON:
			return GameRandom::Range(kTyphoonGustIntervalMin, kTyphoonGustIntervalMax);
		case TyphoonStrength::SEVERE:
			return GameRandom::Range(kSevereGustIntervalMin, kSevereGustIntervalMax);
		case TyphoonStrength::SUPER:
			return GameRandom::Range(kSuperGustIntervalMin, kSuperGustIntervalMax);
		case TyphoonStrength::NONE:
			return 0.0f;
		}
		return 0.0f;
	}

	/** 以发射频率而不是另做贴图区分台风强度，保证视觉浓度只需在此集中调参。 */
	float TyphoonWindParticleInterval(TyphoonStrength strength)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON: return kTyphoonWindParticleInterval;
		case TyphoonStrength::SEVERE:  return kSevereWindParticleInterval;
		case TyphoonStrength::SUPER:   return kSuperWindParticleInterval;
		case TyphoonStrength::NONE:    return 0.0f;
		}
		return 0.0f;
	}

	/** 按当前强度抽取单向衰减阶段时长；台风不会在同一场大雨中反向增强。 */
	float RandomTyphoonStrengthDuration(TyphoonStrength strength)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON:
			return GameRandom::Range(kTyphoonDecayMin, kTyphoonDecayMax);
		case TyphoonStrength::SEVERE:
			return GameRandom::Range(kSevereTyphoonDecayMin, kSevereTyphoonDecayMax);
		case TyphoonStrength::SUPER:
			return GameRandom::Range(kSuperTyphoonDecayMin, kSuperTyphoonDecayMax);
		case TyphoonStrength::NONE:
			return 0.0f;
		}
		return 0.0f;
	}

	/** 返回台风强度对应的顺风/逆风僵尸水平移动倍率。 */
	float TyphoonZombieMoveMultiplier(TyphoonStrength strength, bool tailwind)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON:
			return tailwind ? kTyphoonTailwindZombieMove : kTyphoonHeadwindZombieMove;
		case TyphoonStrength::SEVERE:
			return tailwind ? kSevereTailwindZombieMove : kSevereHeadwindZombieMove;
		case TyphoonStrength::SUPER:
			return tailwind ? kSuperTailwindZombieMove : kSuperHeadwindZombieMove;
		case TyphoonStrength::NONE:
			return 1.0f;
		}
		return 1.0f;
	}

	/** 返回台风强度对应的顺风/逆风轻型植物子弹水平速度倍率。 */
	float TyphoonPlantBulletSpeedMultiplier(TyphoonStrength strength, bool tailwind)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON:
			return tailwind ? kTyphoonTailwindBulletSpeed : kTyphoonHeadwindBulletSpeed;
		case TyphoonStrength::SEVERE:
			return tailwind ? kSevereTailwindBulletSpeed : kSevereHeadwindBulletSpeed;
		case TyphoonStrength::SUPER:
			return tailwind ? kSuperTailwindBulletSpeed : kSuperHeadwindBulletSpeed;
		case TyphoonStrength::NONE:
			return 1.0f;
		}
		return 1.0f;
	}

	/** 返回台风强度对应的顺风/逆风轻型植物子弹命中伤害倍率。 */
	float TyphoonPlantBulletDamageMultiplier(TyphoonStrength strength, bool tailwind)
	{
		switch (strength) {
		case TyphoonStrength::TYPHOON:
			return tailwind ? kTyphoonTailwindBulletDamage : kTyphoonHeadwindBulletDamage;
		case TyphoonStrength::SEVERE:
			return tailwind ? kSevereTailwindBulletDamage : kSevereHeadwindBulletDamage;
		case TyphoonStrength::SUPER:
			return tailwind ? kSuperTailwindBulletDamage : kSuperHeadwindBulletDamage;
		case TyphoonStrength::NONE:
			return 1.0f;
		}
		return 1.0f;
	}

	/** 返回指定雨势在当前后期压力下的僵尸速度倍率，供天气过渡插值复用。 */
	float ZombieSpeedForRain(RainIntensity intensity, float pressureFactor)
	{
		float baseSpeed = 1.0f;
		float pressuredSpeed = 1.0f;
		switch (intensity) {
		case RainIntensity::LIGHT:
			baseSpeed = kLightZombieSpeed;
			pressuredSpeed = kPressuredLightZombieSpeed;
			break;
		case RainIntensity::MEDIUM:
			baseSpeed = kMediumZombieSpeed;
			pressuredSpeed = kPressuredMediumZombieSpeed;
			break;
		case RainIntensity::HEAVY:
			baseSpeed = kHeavyZombieSpeed;
			pressuredSpeed = kPressuredHeavyZombieSpeed;
			break;
		case RainIntensity::CLEAR:
			break;
		}
		const float pressure = std::clamp(pressureFactor, 0.0f, 1.0f);
		return baseSpeed + (pressuredSpeed - baseSpeed) * pressure;
	}

	/** 返回指定雨势在当前压力下的植物行动倍率，供天气过渡插值复用。 */
	float PlantSpeedForRain(RainIntensity intensity, float pressureFactor)
	{
		float baseSpeed = 1.0f;
		float pressuredSpeed = 1.0f;
		switch (intensity) {
		case RainIntensity::LIGHT:
			baseSpeed = kLightPlantActionSpeed;
			pressuredSpeed = kPressuredLightPlantActionSpeed;
			break;
		case RainIntensity::MEDIUM:
			baseSpeed = kMediumPlantActionSpeed;
			pressuredSpeed = kPressuredMediumPlantActionSpeed;
			break;
		case RainIntensity::HEAVY:
			baseSpeed = kHeavyPlantActionSpeed;
			pressuredSpeed = kPressuredHeavyPlantActionSpeed;
			break;
		case RainIntensity::CLEAR:
			break;
		}
		const float pressure = std::clamp(pressureFactor, 0.0f, 1.0f);
		return baseSpeed + (pressuredSpeed - baseSpeed) * pressure;
	}

	/** 返回指定雨势的世界暗幕 alpha，供过渡插值复用。 */
	float OverlayAlphaForRain(RainIntensity intensity)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:  return kLightOverlayAlpha;
		case RainIntensity::MEDIUM: return kMediumOverlayAlpha;
		case RainIntensity::HEAVY:  return kHeavyOverlayAlpha;
		case RainIntensity::CLEAR:  return 0.0f;
		}
		return 0.0f;
	}

	/** 返回指定雨势的雨声请求音量，晴天为 0。 */
	float RainVolumeForIntensity(RainIntensity intensity)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:  return kLightRainVolume;
		case RainIntensity::MEDIUM: return kMediumRainVolume;
		case RainIntensity::HEAVY:  return kHeavyRainVolume;
		case RainIntensity::CLEAR:  return 0.0f;
		}
		return 0.0f;
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

	int RainTransitionWeightTotal(RainIntensity intensity, bool canIntensify,
		bool canHold, float lateFactor)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:  return canIntensify ? kLightTransitionWeightTotal : 0;
		case RainIntensity::MEDIUM:
			return LerpWeatherWeight(kMediumToLightWeight, kLateMediumToLightWeight, lateFactor)
				+ LerpWeatherWeight(kMediumToClearWeight, kLateMediumToClearWeight, lateFactor)
				+ (canHold ? kMediumHoldWeight : 0);
		case RainIntensity::HEAVY:
			return LerpWeatherWeight(kHeavyToMediumWeight, kLateHeavyToMediumWeight, lateFactor)
				+ LerpWeatherWeight(kHeavyToLightWeight, kLateHeavyToLightWeight, lateFactor)
				+ LerpWeatherWeight(kHeavyToClearWeight, kLateHeavyToClearWeight, lateFactor)
				+ (canHold ? kHeavyHoldWeight : 0);
		case RainIntensity::CLEAR:  return 0;
		}
		return 0;
	}

	/** 解析动态权重落点；初始小雨可增强，切档后只会继续衰减。 */
	RainIntensity RainTransitionForRoll(RainIntensity intensity, bool canIntensify,
		bool canHold, float lateFactor, int roll)
	{
		switch (intensity) {
		case RainIntensity::LIGHT:
			if (!canIntensify) return RainIntensity::CLEAR;
			if (roll <= kLightToMediumWeight) return RainIntensity::MEDIUM;
			if (roll <= kLightToMediumWeight + kLightToHeavyWeight) return RainIntensity::HEAVY;
			return RainIntensity::CLEAR;
		case RainIntensity::MEDIUM: {
			const int toLight = LerpWeatherWeight(
				kMediumToLightWeight, kLateMediumToLightWeight, lateFactor);
			const int toClear = LerpWeatherWeight(
				kMediumToClearWeight, kLateMediumToClearWeight, lateFactor);
			if (roll <= toLight) return RainIntensity::LIGHT;
			if (roll <= toLight + toClear) return RainIntensity::CLEAR;
			return canHold ? RainIntensity::MEDIUM : RainIntensity::CLEAR;
		}
		case RainIntensity::HEAVY: {
			const int toMedium = LerpWeatherWeight(
				kHeavyToMediumWeight, kLateHeavyToMediumWeight, lateFactor);
			const int toLight = LerpWeatherWeight(
				kHeavyToLightWeight, kLateHeavyToLightWeight, lateFactor);
			const int toClear = LerpWeatherWeight(
				kHeavyToClearWeight, kLateHeavyToClearWeight, lateFactor);
			if (roll <= toMedium) return RainIntensity::MEDIUM;
			if (roll <= toMedium + toLight) return RainIntensity::LIGHT;
			if (roll <= toMedium + toLight + toClear) return RainIntensity::CLEAR;
			return canHold ? RainIntensity::HEAVY : RainIntensity::CLEAR;
		}
		case RainIntensity::CLEAR:
			return RainIntensity::CLEAR;
		}
		return RainIntensity::CLEAR;
	}

	/** 枚举当前动态权重真正允许的下一天气，错误预报只能从这些候选中选择。 */
	int BuildPlausibleForecasts(RainIntensity current, bool canIntensify, bool canHold,
		float directorFactor, bool forceHeavy, std::array<RainIntensity, 4>& forecasts)
	{
		int count = 0;
		switch (current) {
		case RainIntensity::CLEAR: {
			if (forceHeavy) {
				forecasts[0] = RainIntensity::HEAVY;
				return 1;
			}
			const NewWeatherWeights weights = BuildNewWeatherWeights(directorFactor);
			if (weights.clear > 0) forecasts[count++] = RainIntensity::CLEAR;
			if (weights.light > 0) forecasts[count++] = RainIntensity::LIGHT;
			if (weights.medium > 0) forecasts[count++] = RainIntensity::MEDIUM;
			if (weights.heavy > 0) forecasts[count++] = RainIntensity::HEAVY;
			return count;
		}
		case RainIntensity::LIGHT:
			if (!canIntensify) {
				forecasts[0] = RainIntensity::CLEAR;
				return 1;
			}
			forecasts = { RainIntensity::MEDIUM, RainIntensity::HEAVY, RainIntensity::CLEAR };
			return 3;
		case RainIntensity::MEDIUM: {
			const int toLight = LerpWeatherWeight(
				kMediumToLightWeight, kLateMediumToLightWeight, directorFactor);
			const int toClear = LerpWeatherWeight(
				kMediumToClearWeight, kLateMediumToClearWeight, directorFactor);
			if (toLight > 0) forecasts[count++] = RainIntensity::LIGHT;
			if (toClear > 0) forecasts[count++] = RainIntensity::CLEAR;
			if (canHold && kMediumHoldWeight > 0) forecasts[count++] = RainIntensity::MEDIUM;
			return count;
		}
		case RainIntensity::HEAVY: {
			const int toMedium = LerpWeatherWeight(
				kHeavyToMediumWeight, kLateHeavyToMediumWeight, directorFactor);
			const int toLight = LerpWeatherWeight(
				kHeavyToLightWeight, kLateHeavyToLightWeight, directorFactor);
			const int toClear = LerpWeatherWeight(
				kHeavyToClearWeight, kLateHeavyToClearWeight, directorFactor);
			if (toMedium > 0) forecasts[count++] = RainIntensity::MEDIUM;
			if (toLight > 0) forecasts[count++] = RainIntensity::LIGHT;
			if (toClear > 0) forecasts[count++] = RainIntensity::CLEAR;
			if (canHold && kHeavyHoldWeight > 0) forecasts[count++] = RainIntensity::HEAVY;
			return count;
		}
		}
		return 0;
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
	InitializeCell(IsPoolBackground() ? 5 : 4, 8);
	InitializeRows();
}

Board::~Board()
{
	// 原版 Board.DisposeBoard 无条件 StopFoley(Rain)；离开关卡时同样保证循环声不泄漏到菜单。
	StopRainAudio();
}

float Board::GetZombieRainSpeedMultiplier() const
{
	const float progress = GetWeatherTransitionProgress();
	const float pressure = GetWeatherPressureFactor();
	const float previous = ZombieSpeedForRain(mPreviousRainIntensity, pressure);
	return previous + (ZombieSpeedForRain(mRainIntensity, pressure) - previous) * progress;
}

/** 当前新大雨若进行台风判定时的实际概率，包含波次成长与连续落空保底。 */
int Board::GetCurrentTyphoonChancePercent() const
{
	const int baseChance = LerpWeatherWeight(kTyphoonChanceEarlyPercent,
		kTyphoonChanceLatePercent, GetWeatherDirectorFactor());
	return std::min(kTyphoonChanceMaxPercent,
		baseChance + mHeavyPhasesWithoutTyphoon * kTyphoonPityPerMissPercent);
}

float Board::GetZombieWindMoveMultiplier(bool movingTowardFront) const
{
	if (!HasTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| mWindDirection == WindDirection::NONE) return 1.0f;
	const bool tailwind = (movingTowardFront
		&& mWindDirection == WindDirection::TOWARD_FRONT)
		|| (!movingTowardFront && mWindDirection == WindDirection::TOWARD_HOUSE);
	return TyphoonZombieMoveMultiplier(mTyphoonStrength, tailwind);
}

float Board::GetPlantBulletWindSpeedMultiplier(bool movingTowardFront) const
{
	if (!HasTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| mWindDirection == WindDirection::NONE) return 1.0f;
	const bool tailwind = (movingTowardFront
		&& mWindDirection == WindDirection::TOWARD_FRONT)
		|| (!movingTowardFront && mWindDirection == WindDirection::TOWARD_HOUSE);
	return TyphoonPlantBulletSpeedMultiplier(mTyphoonStrength, tailwind);
}

float Board::GetPlantBulletWindDamageMultiplier(bool movingTowardFront) const
{
	if (!HasTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| mWindDirection == WindDirection::NONE) return 1.0f;
	const bool tailwind = (movingTowardFront
		&& mWindDirection == WindDirection::TOWARD_FRONT)
		|| (!movingTowardFront && mWindDirection == WindDirection::TOWARD_HOUSE);
	return TyphoonPlantBulletDamageMultiplier(mTyphoonStrength, tailwind);
}

bool Board::IsTyphoonGustWarning() const
{
	return HasTyphoon() && !mTyphoonGustActive && mTyphoonGustsRemaining > 0
		&& mWindGustTimer > 0.0f && mWindGustTimer <= kTyphoonGustWarningTime;
}

/**
 * 阵风速度采用 4p(1-p) 包络从零升至峰值再回落；魅惑只改变自主行走方向，
 * 不改变空气对物体施加的物理方向，因此敌对与魅惑僵尸共用同一有符号漂移。
 */
float Board::GetZombieGustDriftVelocity() const
{
	if (!mTyphoonGustActive || mRainIntensity != RainIntensity::HEAVY
		|| mActiveGustDuration <= 0.0f) return 0.0f;
	const float progress = std::clamp(
		1.0f - mActiveGustTimer / mActiveGustDuration, 0.0f, 1.0f);
	const float envelope = 4.0f * progress * (1.0f - progress);
	const float speed = TyphoonGustZombiePeakSpeed(mActiveGustStrength) * envelope;
	return mActiveGustDirection == WindDirection::TOWARD_FRONT ? speed : -speed;
}

float Board::GetZombieGustFrontLimit() const
{
	return static_cast<float>(SCENE_WIDTH) + kGustZombieFrontLimitPadding;
}

float Board::GetPlantRainActionSpeedMultiplier() const
{
	const float progress = GetWeatherTransitionProgress();
	const float pressure = GetWeatherPressureFactor();
	const float previous = PlantSpeedForRain(mPreviousRainIntensity, pressure);
	return previous + (PlantSpeedForRain(mRainIntensity, pressure) - previous) * progress;
}

float Board::GetRainOverlayAlpha() const
{
	const float progress = GetWeatherTransitionProgress();
	const float previous = OverlayAlphaForRain(mPreviousRainIntensity);
	return previous + (OverlayAlphaForRain(mRainIntensity) - previous) * progress;
}

/** 后期强度按波次推进；无尽模式额外按轮次抬高下限，防止新一轮又退回早期天气。 */
float Board::GetWeatherLateGameFactor() const
{
	const float waveProgress = mMaxWave > 0
		? std::clamp(static_cast<float>(mCurrentWave) / static_cast<float>(mMaxWave), 0.0f, 1.0f)
		: 0.0f;
	float overallProgress = waveProgress;
	if (mIsSurvival && kSurvivalLateWeatherFullRound > 1) {
		const float roundProgress = std::clamp(
			static_cast<float>(mSurvivalRound - 1)
				/ static_cast<float>(kSurvivalLateWeatherFullRound - 1), 0.0f, 1.0f);
		overallProgress = std::max(overallProgress, roundProgress);
	}

	const float linear = std::clamp((overallProgress - kLateWeatherRampStart)
		/ (1.0f - kLateWeatherRampStart), 0.0f, 1.0f);
	return linear * linear * (3.0f - 2.0f * linear);
}

/**
 * 返回独立天气压力曲线；玩法倍率直接使用，天气导演再与既有场次成长取较大值。
 * 普通关从 40% 波次进度起成长、75% 时达到完整压力；黑夜无尽从第 8 轮平滑成长到第 20 轮。
 * 该值完全由已持久化的波次/轮次派生，不需要新增存档字段。
 */
float Board::GetWeatherPressureFactor() const
{
	if (!mIsSurvival) {
		if (kAdventurePressureFullProgress <= kLateWeatherRampStart) return 1.0f;
		const float waveProgress = mMaxWave > 0
			? std::clamp(static_cast<float>(mCurrentWave) / static_cast<float>(mMaxWave),
				0.0f, 1.0f)
			: 0.0f;
		const float linear = std::clamp((waveProgress - kLateWeatherRampStart)
			/ (kAdventurePressureFullProgress - kLateWeatherRampStart), 0.0f, 1.0f);
		return linear * linear * (3.0f - 2.0f * linear);
	}
	if (kSurvivalPressureFullRound <= kSurvivalPressureStartRound) return 1.0f;

	const float linear = std::clamp(
		static_cast<float>(mSurvivalRound - kSurvivalPressureStartRound)
			/ static_cast<float>(kSurvivalPressureFullRound - kSurvivalPressureStartRound),
		0.0f, 1.0f);
	return linear * linear * (3.0f - 2.0f * linear);
}

/** 天气导演取既有出现场次成长与独立压力曲线的较大值，避免任何旧后期关卡倒退。 */
float Board::GetWeatherDirectorFactor() const
{
	return std::max(GetWeatherLateGameFactor(), GetWeatherPressureFactor());
}

/** 后期更可靠但仍保留误报；主人的平衡上限固定为 90%。 */
int Board::GetCurrentWeatherForecastAccuracyPercent() const
{
	return LerpWeatherWeight(kWeatherForecastAccuracyPercent,
		kLateWeatherForecastAccuracyPercent, GetWeatherDirectorFactor());
}

int Board::GetCurrentNewWeatherWeight(RainIntensity intensity) const
{
	const NewWeatherWeights weights = BuildNewWeatherWeights(GetWeatherDirectorFactor());
	switch (intensity) {
	case RainIntensity::CLEAR:  return weights.clear;
	case RainIntensity::LIGHT:  return weights.light;
	case RainIntensity::MEDIUM: return weights.medium;
	case RainIntensity::HEAVY:  return weights.heavy;
	}
	return 0;
}

/** 满足导演阈值且上一轮为弱天气时，下一次晴天出发的天气前沿强制为大雨。 */
bool Board::ShouldForceHeavyWeather() const
{
	return mRainIntensity == RainIntensity::CLEAR
		&& GetWeatherDirectorFactor() >= kWeakWeatherPityStart
		&& mWeakWeatherPhasesSinceHeavy >= kWeakWeatherPityMax;
}

int Board::GetNextWeatherRollTotal() const
{
	if (mRainIntensity == RainIntensity::CLEAR) {
		if (ShouldForceHeavyWeather()) return 1;
		return BuildNewWeatherWeights(GetWeatherDirectorFactor()).Total();
	}
	return RainTransitionWeightTotal(mRainIntensity, mRainCanIntensify, mRainCanHold,
		GetWeatherDirectorFactor());
}

/** 只在晴天揭晓一个新天气前沿时记账；前期结果不会预存成后期保底。 */
void Board::RecordNewWeatherOutcome(RainIntensity next)
{
	if (GetWeatherDirectorFactor() < kWeakWeatherPityStart) {
		mWeakWeatherPhasesSinceHeavy = 0;
		return;
	}
	if (next == RainIntensity::HEAVY) {
		mWeakWeatherPhasesSinceHeavy = 0;
		return;
	}
	mWeakWeatherPhasesSinceHeavy = std::min(
		mWeakWeatherPhasesSinceHeavy + 1, kWeakWeatherPityMax);
}

/** 返回当前两秒天气过渡的平滑进度；无过渡时视为已经到达目标雨势。 */
float Board::GetWeatherTransitionProgress() const
{
	if (mWeatherTransitionTimer <= 0.0f) return 1.0f;
	const float linear = std::clamp(1.0f
		- mWeatherTransitionTimer / kWeatherTransitionDuration, 0.0f, 1.0f);
	return linear * linear * (3.0f - 2.0f * linear);
}

/** 雨声音量与暗幕、玩法倍率共用同一平滑进度。 */
float Board::GetRainAudioVolume() const
{
	const float progress = GetWeatherTransitionProgress();
	const float previous = RainVolumeForIntensity(mPreviousRainIntensity);
	return previous + (RainVolumeForIntensity(mRainIntensity) - previous) * progress;
}

/** 立即把天气枚举切到目标，同时保留旧雨势供后续两秒插值。 */
void Board::BeginWeatherTransition(RainIntensity target)
{
	mPreviousRainIntensity = mRainIntensity;
	mRainIntensity = target;
	mWeatherTransitionTimer = mPreviousRainIntensity == target
		? 0.0f : kWeatherTransitionDuration;
}

/** 从存档恢复过渡；旧档、同雨势或归零状态直接按目标天气稳定显示。 */
void Board::RestoreWeatherTransition(RainIntensity previous, float remaining)
{
	mPreviousRainIntensity = previous;
	mWeatherTransitionTimer = std::clamp(remaining, 0.0f, kWeatherTransitionDuration);
	if (mWeatherTransitionTimer <= 0.0f || mPreviousRainIntensity == mRainIntensity) {
		mPreviousRainIntensity = mRainIntensity;
		mWeatherTransitionTimer = 0.0f;
	}
}

/** 推进倍率、暗幕和雨声的统一过渡；结束放晴过渡后才真正停止循环雨声。 */
void Board::UpdateWeatherTransition(float deltaTime)
{
	if (mWeatherTransitionTimer <= 0.0f) return;
	mWeatherTransitionTimer = std::max(0.0f, mWeatherTransitionTimer - deltaTime);
	RefreshZombieWeatherSpeeds();

	if (mRainIntensity != RainIntensity::CLEAR
		|| mPreviousRainIntensity != RainIntensity::CLEAR) {
		StartRainAudio();
	}
	if (mWeatherTransitionTimer <= 0.0f) {
		mPreviousRainIntensity = mRainIntensity;
		RefreshZombieWeatherSpeeds();
		if (mRainIntensity == RainIntensity::CLEAR) StopRainAudio();
		else StartRainAudio();
	}
}

/** AutoTest 固定状态使用：跳过视觉过渡并立即应用目标天气的最终倍率与声音。 */
void Board::FinishWeatherTransitionImmediately()
{
	mPreviousRainIntensity = mRainIntensity;
	mWeatherTransitionTimer = 0.0f;
	RefreshZombieWeatherSpeeds();
	if (mRainIntensity == RainIntensity::CLEAR) StopRainAudio();
	else StartRainAudio();
}

void Board::InitializeWeather()
{
	if (mWeatherInitialized) return;
	// 主进度由“当前雨势 + 一个复用倒计时”驱动：CLEAR 时倒计时代表距首场雨/下一场雨，
	// 下雨时则代表当前雨段剩余时间；另用布尔值记录初始小雨是否还拥有一次增强资格。
	// 第一大关与白天无尽把倒计时保持为 0；冒险第二大关起均启用天气。
	mWeatherInitialized = true;
	mRainIntensity = RainIntensity::CLEAR;
	mPreviousRainIntensity = RainIntensity::CLEAR;
	mForecastRainIntensity = RainIntensity::CLEAR;
	mActualForecastRainIntensity = RainIntensity::CLEAR;
	mLightningTimer = 0.0f;
	mRainSplashTimer = 0.0f;
	mRainCanIntensify = false;
	mRainCanHold = false;
	mWeatherTransitionTimer = 0.0f;
	mWeatherForecastReady = false;
	ClearPendingHeavyRainWarning();
	mRainVisualActive = false;
	mRainVisualEffectName.clear();
	mWeakWeatherPhasesSinceHeavy = 0;
	mHeavyPhasesWithoutTyphoon = 0;
	StopTyphoon();
	mWeatherTimer = SupportsWeather()
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
	const std::string effectName = RainEffectName(mRainIntensity, mWindDirection);
	if (effectName.empty()) return;
	if (!mRainVisualEffectName.empty() && mRainVisualEffectName != effectName) {
		// 风向或雨势切换时只停旧雨的发射器；在途雨丝保留到自然淡出。
		g_particleSystem->StopEffect(mRainVisualEffectName);
	}
	// Box 发射器以屏幕上沿中央为基准铺满逻辑画面；运行期时长覆盖 XML 上限，
	// 使随机雨长与读档剩余时间都能和玩法倍率同步结束。
	g_particleSystem->EmitEffect(effectName,
		Vector(static_cast<float>(SCENE_WIDTH) * 0.5f, -60.0f),
		LAYER_EFFECTS_WORLD, duration);
	mRainVisualActive = true;
	mRainVisualEffectName = effectName;
}

/** 台风开始、结束或翻向后按剩余雨时无缝切换定向雨丝。 */
void Board::RestartRainVisualForWindChange()
{
	if (mRainIntensity != RainIntensity::HEAVY || mWeatherTimer <= 0.0f) return;
	mRainVisualActive = false;
	EmitRainEffect(mWeatherTimer);
}

/** 在草地逻辑网格内随机选择落点，播放短促的原版雨滴水花与扩散圆圈。 */
void Board::TriggerRainGroundSplash()
{
	if (!g_particleSystem || mRows <= 0 || mColumns <= 0) return;

	// 用完整网格边界而非窗口随机值，既覆盖战场又给 33px 水花留下屏内余量。
	const float minX = CELL_INITALIZE_POS_X + kRainSplashEdgePadding;
	const float maxX = CELL_INITALIZE_POS_X + mColumns * CELL_COLLIDER_SIZE_X
		- kRainSplashEdgePadding;
	const float minY = mCellInitialY + kRainSplashEdgePadding;
	const float maxY = mCellInitialY + mRows * mCellHeight
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
		&& g_particleSystem->IsEffectEmitting(
			RainEffectName(mRainIntensity, mWindDirection));
}

void Board::StartRainAudio()
{
	if (mRainIntensity == RainIntensity::CLEAR
		&& (mWeatherTransitionTimer <= 0.0f
			|| mPreviousRainIntensity == RainIntensity::CLEAR)) return;
	// 原版 FoleyType.Rain 绑定 SOUND_RAIN 且带 Loop 标志；这里只把音量按雨势分层。
	AudioSystem::PlayLoopingSound(ResourceKeys::Sounds::SOUND_RAIN, GetRainAudioVolume());
}

void Board::StopRainAudio()
{
	AudioSystem::StopLoopingSound(ResourceKeys::Sounds::SOUND_RAIN);
}

/** 按当前阶段规则抽取下一天气；只由预警准备阶段调用一次。 */
RainIntensity Board::RollNextWeather(int forcedRoll)
{
	const float directorFactor = GetWeatherDirectorFactor();
	if (mRainIntensity == RainIntensity::CLEAR) {
		if (ShouldForceHeavyWeather()) return RainIntensity::HEAVY;
		const NewWeatherWeights weights = BuildNewWeatherWeights(directorFactor);
		const int total = weights.Total();
		const int roll = forcedRoll > 0 ? std::clamp(forcedRoll, 1, total)
			: GameRandom::Range(1, total);
		if (roll <= weights.clear) return RainIntensity::CLEAR;
		if (roll <= weights.clear + weights.light) return RainIntensity::LIGHT;
		if (roll <= weights.clear + weights.light + weights.medium) return RainIntensity::MEDIUM;
		return RainIntensity::HEAVY;
	}

	const int total = RainTransitionWeightTotal(
		mRainIntensity, mRainCanIntensify, mRainCanHold, directorFactor);
	if (total <= 0) return RainIntensity::CLEAR;
	const int roll = forcedRoll > 0 ? std::clamp(forcedRoll, 1, total)
		: GameRandom::Range(1, total);
	return RainTransitionForRoll(
		mRainIntensity, mRainCanIntensify, mRainCanHold, directorFactor, roll);
}

/** 锁定真实天气，并按 75%～90% 动态准确率生成公开预报。 */
void Board::PrepareWeatherForecast(int weatherRoll)
{
	if (mWeatherForecastReady) return;
	ClearPendingHeavyRainWarning();
	mActualForecastRainIntensity = RollNextWeather(weatherRoll);
	mForecastRainIntensity = mActualForecastRainIntensity;

	// 错误预报仍须来自当前非零权重候选；弱天气保底或确定性尾段没有错误候选时强制报准。
	if (GameRandom::Range(1, 100) > GetCurrentWeatherForecastAccuracyPercent()) {
		std::array<RainIntensity, 4> plausible{};
		const int plausibleCount = BuildPlausibleForecasts(
			mRainIntensity, mRainCanIntensify, mRainCanHold,
			GetWeatherDirectorFactor(), ShouldForceHeavyWeather(), plausible);
		std::array<RainIntensity, 4> wrongForecasts{};
		int wrongCount = 0;
		for (int i = 0; i < plausibleCount; ++i) {
			if (plausible[i] != mActualForecastRainIntensity) {
				wrongForecasts[wrongCount++] = plausible[i];
			}
		}
		if (wrongCount > 0) {
			mForecastRainIntensity = wrongForecasts[GameRandom::Range(0, wrongCount - 1)];
		}
	}
	mWeatherForecastReady = true;
	PreparePendingHeavyTyphoon();
}

/**
 * 在公开预报锁定后提前锁定下一场大雨的完整台风初始状态。
 * 这里只消费随机数，不提前改变当前天气、台风保底或玩法倍率；真正切档时才兑现结果。
 */
void Board::PreparePendingHeavyTyphoon(int chanceRoll, int strengthRoll)
{
	if (mPendingHeavyTyphoonPrepared || !mWeatherForecastReady
		|| mActualForecastRainIntensity != RainIntensity::HEAVY
		|| mRainIntensity == RainIntensity::HEAVY) return;

	mPendingHeavyTyphoonPrepared = true;
	mPendingHeavyTyphoonStrength = TyphoonStrength::NONE;
	mPendingHeavyWindDirection = WindDirection::NONE;
	mPendingHeavyTyphoonStrengthTimer = 0.0f;
	mPendingHeavyWindDirectionTimer = 0.0f;
	mPendingHeavyWindGustTimer = 0.0f;
	mPendingHeavyTyphoonGustsRemaining = 0;
	mPendingHeavyRainPromptVariant = GameRandom::Range(0, 2);

	const int chance = GetCurrentTyphoonChancePercent();
	if (chanceRoll <= 0) chanceRoll = GameRandom::Range(1, 100);
	if (chanceRoll > chance) return;

	const TyphoonWeights weights = BuildTyphoonWeights(GetWeatherDirectorFactor());
	const int totalWeight = weights.Total();
	const int roll = strengthRoll > 0
		? std::clamp(strengthRoll, 1, totalWeight)
		: GameRandom::Range(1, totalWeight);
	if (roll <= weights.normal) {
		mPendingHeavyTyphoonStrength = TyphoonStrength::TYPHOON;
	}
	else if (roll <= weights.normal + weights.severe) {
		mPendingHeavyTyphoonStrength = TyphoonStrength::SEVERE;
	}
	else {
		mPendingHeavyTyphoonStrength = TyphoonStrength::SUPER;
	}
	mPendingHeavyWindDirection = WindDirectionForRoll(0);
	mPendingHeavyTyphoonStrengthTimer =
		RandomTyphoonStrengthDuration(mPendingHeavyTyphoonStrength);
	mPendingHeavyWindDirectionTimer = GameRandom::Range(
		kWindDirectionDurationMin, kWindDirectionDurationMax);
	mPendingHeavyTyphoonGustsRemaining =
		TyphoonMaxGusts(mPendingHeavyTyphoonStrength);
	mPendingHeavyWindGustTimer = mPendingHeavyTyphoonGustsRemaining > 0
		? RandomTyphoonGustInterval(mPendingHeavyTyphoonStrength) : 0.0f;
}

/** 清除上一份大雨预警及其待生效台风状态，避免后续天气误消费旧结果。 */
void Board::ClearPendingHeavyRainWarning()
{
	mPendingHeavyTyphoonPrepared = false;
	mPendingHeavyTyphoonStrength = TyphoonStrength::NONE;
	mPendingHeavyWindDirection = WindDirection::NONE;
	mPendingHeavyTyphoonStrengthTimer = 0.0f;
	mPendingHeavyWindDirectionTimer = 0.0f;
	mPendingHeavyWindGustTimer = 0.0f;
	mPendingHeavyTyphoonGustsRemaining = 0;
	mPendingHeavyRainPromptVariant = 0;
	mHeavyRainPromptShown = false;
}

/** 在倒计时进入最后 5 个游戏秒时显示一次已锁定等级的风暴警报。 */
void Board::MaybeShowHeavyRainPrompt()
{
	if (mHeavyRainPromptShown || !mWeatherForecastReady
		|| mActualForecastRainIntensity != RainIntensity::HEAVY
		|| mRainIntensity == RainIntensity::HEAVY
		|| mWeatherTimer > kHeavyRainPromptLeadTime || !mGameScene) return;
	if (!mPendingHeavyTyphoonPrepared) PreparePendingHeavyTyphoon();
	if (!mPendingHeavyTyphoonPrepared) return;
	mGameScene->ShowHeavyRainWarning(
		mPendingHeavyTyphoonStrength, mPendingHeavyRainPromptVariant);
	mHeavyRainPromptShown = true;
}

/** 检查公开预报是否落在当前状态机的合法候选中，供界面诊断与 AutoTest 使用。 */
bool Board::IsWeatherForecastPlausible() const
{
	if (!mWeatherForecastReady) return true;
	std::array<RainIntensity, 4> plausible{};
	const int plausibleCount = BuildPlausibleForecasts(
		mRainIntensity, mRainCanIntensify, mRainCanHold,
		GetWeatherDirectorFactor(), ShouldForceHeavyWeather(), plausible);
	return std::find(plausible.begin(), plausible.begin() + plausibleCount,
		mForecastRainIntensity) != plausible.begin() + plausibleCount;
}

/** 揭晓锁定的真实天气；预报错误时通知场景显示非模态失败提示。 */
void Board::ConsumeWeatherForecast()
{
	if (!mWeatherForecastReady) PrepareWeatherForecast();
	const RainIntensity forecast = mForecastRainIntensity;
	const RainIntensity next = mActualForecastRainIntensity;
	if (forecast != next && mGameScene) {
		mGameScene->ShowWeatherForecastFailure(forecast, next);
	}
	mWeatherForecastReady = false;
	mForecastRainIntensity = RainIntensity::CLEAR;
	mActualForecastRainIntensity = RainIntensity::CLEAR;

	if (mRainIntensity == RainIntensity::CLEAR) {
		RecordNewWeatherOutcome(next);
		if (next == RainIntensity::CLEAR) {
			// 损坏存档或未知枚举的保守兜底：重新进入晴空间隔，避免空雨段循环。
			EndRain();
			return;
		}
		BeginRain(next, RandomNewRainDuration(next, GetWeatherDirectorFactor()),
			next == RainIntensity::LIGHT, true);
		return;
	}

	if (next == RainIntensity::CLEAR) {
		EndRain();
		return;
	}
	// 同档续期或真正切档都会消费本场唯一续期资格，后续只走有界衰减链。
	BeginRain(next, RandomTailRainDuration(next, GetWeatherDirectorFactor()), false, false);
}

/**
 * 新大雨阶段只判定一次是否附加台风，并锁定强度、初始方向和本阶段阵风预算。
 * 正式流程传 0 使用随机点数；AutoTest 可传 1-based 固定点数验证概率、保底和强度边界。
 */
void Board::StartTyphoonForHeavyPhase(int chanceRoll, int strengthRoll,
	WindDirection forcedDirection)
{
	StopTyphoon();
	const int chance = GetCurrentTyphoonChancePercent();
	if (chanceRoll <= 0) chanceRoll = GameRandom::Range(1, 100);
	if (chanceRoll > chance) {
		mHeavyPhasesWithoutTyphoon = std::min(
			mHeavyPhasesWithoutTyphoon + 1, kTyphoonPityMaxMisses);
		return;
	}
	mHeavyPhasesWithoutTyphoon = 0;

	const TyphoonWeights weights = BuildTyphoonWeights(GetWeatherDirectorFactor());
	const int totalWeight = weights.Total();
	const int roll = strengthRoll > 0 ? strengthRoll : GameRandom::Range(1, totalWeight);
	if (roll <= weights.normal) {
		mTyphoonStrength = TyphoonStrength::TYPHOON;
	}
	else if (roll <= weights.normal + weights.severe) {
		mTyphoonStrength = TyphoonStrength::SEVERE;
	}
	else {
		mTyphoonStrength = TyphoonStrength::SUPER;
	}
	const bool validForcedDirection = forcedDirection == WindDirection::TOWARD_HOUSE
		|| forcedDirection == WindDirection::TOWARD_FRONT;
	mWindDirection = validForcedDirection ? forcedDirection : WindDirectionForRoll(0);
	mTyphoonStrengthTimer = RandomTyphoonStrengthDuration(mTyphoonStrength);
	mWindDirectionTimer = GameRandom::Range(
		kWindDirectionDurationMin, kWindDirectionDurationMax);
	mTyphoonGustsRemaining = TyphoonMaxGusts(mTyphoonStrength);
	mWindGustTimer = mTyphoonGustsRemaining > 0
		? RandomTyphoonGustInterval(mTyphoonStrength) : 0.0f;
	RefreshZombieWeatherSpeeds();
}

/**
 * 把预警期锁定的台风完整状态兑现到刚开始的大雨。
 * 台风保底只在此处更新，因此退出或读档不会把尚未来临的大雨提前计入结果。
 */
void Board::ConsumePendingHeavyTyphoon()
{
	if (!mPendingHeavyTyphoonPrepared) {
		StartTyphoonForHeavyPhase();
		return;
	}
	const TyphoonStrength strength = mPendingHeavyTyphoonStrength;
	const WindDirection direction = mPendingHeavyWindDirection;
	const float strengthTimer = mPendingHeavyTyphoonStrengthTimer;
	const float gustTimer = mPendingHeavyWindGustTimer;
	const float directionTimer = mPendingHeavyWindDirectionTimer;
	const int gustsRemaining = mPendingHeavyTyphoonGustsRemaining;
	ClearPendingHeavyRainWarning();

	if (strength == TyphoonStrength::NONE) {
		StopTyphoon();
		mHeavyPhasesWithoutTyphoon = std::min(
			mHeavyPhasesWithoutTyphoon + 1, kTyphoonPityMaxMisses);
		return;
	}
	mHeavyPhasesWithoutTyphoon = 0;
	RestoreTyphoonState(strength, direction,
		strengthTimer, gustTimer, directionTimer, gustsRemaining);
}

/** 夹紧并恢复会决定下一轮是否强制大雨的连续弱天气次数。 */
void Board::RestoreWeakWeatherPity(int weakWeatherPhases)
{
	mWeakWeatherPhasesSinceHeavy = std::clamp(
		weakWeatherPhases, 0, kWeakWeatherPityMax);
}

/** 夹紧并恢复会影响下次台风随机判定的连续落空次数。 */
void Board::RestoreTyphoonPity(int missedHeavyPhases)
{
	mHeavyPhasesWithoutTyphoon = std::clamp(
		missedHeavyPhases, 0, kTyphoonPityMaxMisses);
}

/** 从存档恢复尚未生效的大雨台风结果；损坏组合只清空，不会在读档阶段重 roll。 */
void Board::RestorePendingHeavyTyphoon(bool prepared, TyphoonStrength strength,
	WindDirection direction, float strengthTimer, float gustTimer,
	float directionTimer, int gustsRemaining, int promptVariant)
{
	ClearPendingHeavyRainWarning();
	if (!prepared || !mWeatherForecastReady
		|| mActualForecastRainIntensity != RainIntensity::HEAVY
		|| mRainIntensity == RainIntensity::HEAVY) return;
	const bool validStrength = strength >= TyphoonStrength::NONE
		&& strength <= TyphoonStrength::SUPER;
	const bool validDirection = strength == TyphoonStrength::NONE
		? direction == WindDirection::NONE
		: (direction == WindDirection::TOWARD_HOUSE
			|| direction == WindDirection::TOWARD_FRONT);
	if (!validStrength || !validDirection) return;

	mPendingHeavyTyphoonPrepared = true;
	mPendingHeavyTyphoonStrength = strength;
	mPendingHeavyWindDirection = direction;
	mPendingHeavyTyphoonStrengthTimer = strength == TyphoonStrength::NONE
		? 0.0f : std::max(0.0f, strengthTimer);
	mPendingHeavyWindDirectionTimer = strength == TyphoonStrength::NONE
		? 0.0f : std::max(0.0f, directionTimer);
	mPendingHeavyTyphoonGustsRemaining = strength == TyphoonStrength::NONE
		? 0 : std::clamp(gustsRemaining, 0, TyphoonMaxGusts(strength));
	mPendingHeavyWindGustTimer = mPendingHeavyTyphoonGustsRemaining > 0
		? std::max(0.0f, gustTimer) : 0.0f;
	mPendingHeavyRainPromptVariant = std::clamp(promptVariant, 0, 2);
}

/** 夹紧并恢复当前波已经成功生成的精英舞王数量。 */
void Board::RestoreEliteDancerWaveSpawnCount(int count)
{
	mEliteDancersSpawnedThisWave = std::clamp(count, 0, kEliteDancerMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的加固铁门数量。 */
void Board::RestoreReinforcedDoorWaveSpawnCount(int count)
{
	mReinforcedDoorsSpawnedThisWave = std::clamp(count, 0, kReinforcedDoorMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的精英撑杆数量。 */
void Board::RestoreElitePolevaulterWaveSpawnCount(int count)
{
	mElitePolevaultersSpawnedThisWave = std::clamp(count, 0, kElitePolevaulterMaxPerWave);
}

/** 清空全部台风派生状态；中雨、小雨、晴天和旧档默认都以此为单位元。 */
void Board::StopTyphoon()
{
	mWindParticleTimer = 0.0f;
	mTyphoonStrength = TyphoonStrength::NONE;
	mWindDirection = WindDirection::NONE;
	mTyphoonStrengthTimer = 0.0f;
	mWindDirectionTimer = 0.0f;
	mWindGustTimer = 0.0f;
	mTyphoonGustsRemaining = 0;
	mTyphoonGustActive = false;
	mActiveGustStrength = TyphoonStrength::NONE;
	mActiveGustDirection = WindDirection::NONE;
	mActiveGustDuration = 0.0f;
	mActiveGustTimer = 0.0f;
	mActiveGustPlantMoveTimer = 0.0f;
	mActiveGustPlantMoved = false;
	mLastTyphoonMovedPlants = 0;
	mLastTyphoonLostPlants = 0;
	RefreshZombieWeatherSpeeds();
}

/** 从存档恢复已经判定过的台风结果；无效组合只会安全退化为无台风，不重新随机。 */
void Board::RestoreTyphoonState(TyphoonStrength strength, WindDirection direction,
	float strengthTimer, float gustTimer, float directionTimer, int gustsRemaining)
{
	StopTyphoon();
	const bool validStrength = strength == TyphoonStrength::TYPHOON
		|| strength == TyphoonStrength::SEVERE || strength == TyphoonStrength::SUPER;
	const bool validDirection = direction == WindDirection::TOWARD_HOUSE
		|| direction == WindDirection::TOWARD_FRONT;
	if (mRainIntensity != RainIntensity::HEAVY || !validStrength || !validDirection) return;

	mTyphoonStrength = strength;
	mWindDirection = direction;
	mTyphoonStrengthTimer = std::max(0.0f, strengthTimer);
	mWindGustTimer = std::max(0.0f, gustTimer);
	mWindDirectionTimer = std::max(0.0f, directionTimer);
	mTyphoonGustsRemaining = std::clamp(gustsRemaining, 0, TyphoonMaxGusts(strength));
	RefreshZombieWeatherSpeeds();
}

/**
 * 恢复一场已经开始的阵风。锁定值必须与当前台风一致；旧档和损坏组合保持非阵风状态，
 * 未结算植物的剩余时刻夹在阵风余时内，保证读档后至多结算一次。
 */
void Board::RestoreActiveTyphoonGust(bool active, TyphoonStrength strength,
	WindDirection direction, float duration, float remaining,
	float plantMoveRemaining, bool plantMoved)
{
	if (!active || !HasTyphoon() || strength != mTyphoonStrength
		|| direction != mWindDirection || TyphoonGustDistance(strength) <= 0
		|| duration <= 0.0f || remaining <= 0.0f) return;
	mTyphoonGustActive = true;
	mActiveGustStrength = strength;
	mActiveGustDirection = direction;
	mActiveGustDuration = duration;
	mActiveGustTimer = std::clamp(remaining, 0.0f, duration);
	mActiveGustPlantMoved = plantMoved;
	mActiveGustPlantMoveTimer = plantMoved ? 0.0f
		: std::clamp(plantMoveRemaining, 0.0f, mActiveGustTimer);
	mWindGustTimer = 0.0f;
}

/**
 * 台风只沿 SUPER→SEVERE→TYPHOON→NONE 衰减。风向保持不变，阵风预算只会被新上限截断而不补充，
 * 避免衰减反而给玩家追加惩罚；倍率与风线浓度从下一帧起自动采用新强度。
 */
void Board::WeakenTyphoon()
{
	if (!HasTyphoon()) return;
	TyphoonStrength next = TyphoonStrength::NONE;
	switch (mTyphoonStrength) {
	case TyphoonStrength::SUPER:    next = TyphoonStrength::SEVERE; break;
	case TyphoonStrength::SEVERE:   next = TyphoonStrength::TYPHOON; break;
	case TyphoonStrength::TYPHOON:
	case TyphoonStrength::NONE:     next = TyphoonStrength::NONE; break;
	}

	if (next == TyphoonStrength::NONE) {
		StopTyphoon();
		RestartRainVisualForWindChange();
		if (mGameScene) mGameScene->ShowCurrentWeatherNotice();
		return;
	}
	mTyphoonStrength = next;
	mTyphoonStrengthTimer = RandomTyphoonStrengthDuration(next);
	mTyphoonGustsRemaining = std::min(mTyphoonGustsRemaining, TyphoonMaxGusts(next));
	mWindGustTimer = mTyphoonGustsRemaining > 0
		? RandomTyphoonGustInterval(next) : 0.0f;
	mWindParticleTimer = 0.0f;
	RefreshZombieWeatherSpeeds();
	if (mGameScene) mGameScene->ShowCurrentWeatherNotice();
}

/**
 * 将正式波次选中的普通舞王按当前黑夜强天气变异为精英舞王。
 * 每波至多成功三次；失败点数不消费上限，天气减弱后既有精英仍保留。
 */
ZombieType Board::ResolveRainMutationType(ZombieType selected, int mutationRoll)
{
	if (selected != ZombieType::ZOMBIE_DANCER
		|| !GameAPP::GetInstance().GetBackgroundIsNight(mBackGround)
		|| mRainIntensity != RainIntensity::HEAVY
		|| mTyphoonStrength == TyphoonStrength::NONE) {
		return selected;
	}

	const int roll = mutationRoll > 0 ? mutationRoll : GameRandom::Range(1, 100);
	if (roll < 1 || roll > 100 || roll > kEliteDancerMutationChancePercent) return selected;
	// 已达上限的成功变异候选直接作废，由正式挑选循环继续抽取；不能退回普通舞王占掉本次刷新。
	if (mEliteDancersSpawnedThisWave >= kEliteDancerMaxPerWave) {
		return ZombieType::NUM_ZOMBIE_TYPES;
	}
	++mEliteDancersSpawnedThisWave;
	return ZombieType::ZOMBIE_ELITE_DANCER;
}

/**
 * 解析正式波次候选。超过每波上限时返回 NUM_ZOMBIE_TYPES，调用方继续挑选且不消耗预算。
 */
ZombieType Board::ResolveWaveZombieType(ZombieType selected, int mutationRoll)
{
	if (selected == ZombieType::ZOMBIE_REINFORCED_DOOR) {
		if (mReinforcedDoorsSpawnedThisWave >= kReinforcedDoorMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mReinforcedDoorsSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ELITE_POLEVAULTER) {
		if (mElitePolevaultersSpawnedThisWave >= kElitePolevaulterMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mElitePolevaultersSpawnedThisWave;
	}
	return ResolveRainMutationType(selected, mutationRoll);
}

/**
 * 风向维持一段时间后独立重抽；允许继续吹向当前方向，避免固定左右交替被玩家预测。
 * 不在每次阵风临时重抽，玩家仍可根据面板中的实时方向应对当前阵风。
 */
void Board::RerollWindDirection(int directionRoll)
{
	if (!HasTyphoon()) return;
	const WindDirection previousDirection = mWindDirection;
	mWindDirection = WindDirectionForRoll(directionRoll);
	mWindDirectionTimer = GameRandom::Range(
		kWindDirectionDurationMin, kWindDirectionDurationMax);
	if (mWindDirection == previousDirection) return;
	// 下一帧立即发射新方向的风线；旧方向粒子会在自身不足 1.25 秒的寿命内自然淡出。
	mWindParticleTimer = 0.0f;
	RestartRainVisualForWindChange();
}

/**
 * 周期性发射覆盖画面的横向风线。三档共用同一粒子资源，只用发射间隔表达浓度；
 * 该计时器是纯视觉瞬态，读档后从零开始即可按已恢复的强度与方向重建。
 */
void Board::UpdateTyphoonWindVisual(float deltaTime)
{
	if (!g_particleSystem || !HasTyphoon() || mWindDirection == WindDirection::NONE) return;
	mWindParticleTimer -= deltaTime;
	if (mWindParticleTimer > 0.0f) return;

	const float originX = mWindDirection == WindDirection::TOWARD_FRONT
		? -kWindParticleOriginPadding
		: static_cast<float>(SCENE_WIDTH) + kWindParticleOriginPadding;
	g_particleSystem->EmitEffect(WindEffectName(mWindDirection),
		Vector(originX, static_cast<float>(SCENE_HEIGHT) * 0.5f), LAYER_EFFECTS_WORLD);
	mWindParticleTimer = TyphoonWindParticleInterval(mTyphoonStrength);
}

/**
 * 推进台风风向、阵风等待与活动阶段。活动阵风锁定强度和风向，因而衰减/转向计时暂缓；
 * 阵风预算耗尽后持续风仍会影响僵尸自主移动与轻型子弹。
 */
void Board::UpdateTyphoon(float deltaTime)
{
	if (!HasTyphoon()) return;
	if (mRainIntensity != RainIntensity::HEAVY) {
		StopTyphoon();
		return;
	}
	UpdateTyphoonWindVisual(deltaTime);
	if (mTyphoonGustActive) {
		UpdateActiveTyphoonGust(deltaTime);
		return;
	}
	mTyphoonStrengthTimer -= deltaTime;
	if (mTyphoonStrengthTimer <= 0.0f) WeakenTyphoon();
	if (!HasTyphoon()) return;

	mWindDirectionTimer -= deltaTime;
	if (mWindDirectionTimer <= 0.0f) RerollWindDirection();
	if (mTyphoonGustsRemaining <= 0) {
		mWindGustTimer = 0.0f;
		return;
	}

	mWindGustTimer -= deltaTime;
	if (mWindGustTimer > 0.0f) return;
	BeginTyphoonGust(true);
}

/**
 * 启动一次短阵风并一次性抽好植物受力时刻。每帧只推进确定的计时，不重复掷概率，
 * 因而不同帧率和存读档都不会改变本次阵风是否/何时移动植物。
 */
bool Board::BeginTyphoonGust(bool consumeBudget, float forcedPlantMoveIn)
{
	if (!HasTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| mWindDirection == WindDirection::NONE || mTyphoonGustActive) return false;
	mLastTyphoonMovedPlants = 0;
	mLastTyphoonLostPlants = 0;
	const float duration = TyphoonGustDuration(mTyphoonStrength);
	if (duration <= 0.0f) return true;
	if (consumeBudget) {
		if (mTyphoonGustsRemaining <= 0) return false;
		--mTyphoonGustsRemaining;
	}

	mTyphoonGustActive = true;
	mActiveGustStrength = mTyphoonStrength;
	mActiveGustDirection = mWindDirection;
	mActiveGustDuration = duration;
	mActiveGustTimer = duration;
	mActiveGustPlantMoveTimer = forcedPlantMoveIn >= 0.0f
		? std::clamp(forcedPlantMoveIn, 0.0f, duration)
		: duration * GameRandom::Range(kGustPlantMoveProgressMin, kGustPlantMoveProgressMax);
	mActiveGustPlantMoved = false;
	mWindGustTimer = 0.0f;
	if (mActiveGustPlantMoveTimer <= 0.0f) {
		TriggerTyphoonPlantMove(mActiveGustStrength, mActiveGustDirection);
		mActiveGustPlantMoved = true;
	}
	return true;
}

/** 推进活动阵风；先跨越并结算随机植物时刻，再结束阵风，避免长帧漏掉整格位移。 */
void Board::UpdateActiveTyphoonGust(float deltaTime)
{
	if (!mTyphoonGustActive) return;
	if (!mActiveGustPlantMoved) {
		mActiveGustPlantMoveTimer = std::max(0.0f,
			mActiveGustPlantMoveTimer - deltaTime);
		if (mActiveGustPlantMoveTimer <= 0.0f) {
			TriggerTyphoonPlantMove(mActiveGustStrength, mActiveGustDirection);
			mActiveGustPlantMoved = true;
		}
	}
	mActiveGustTimer = std::max(0.0f, mActiveGustTimer - deltaTime);
	if (mActiveGustTimer <= 0.0f) EndTyphoonGust();
}

/** 结束活动阵风并按当前衰减档位安排下一次；预算耗尽则只保留持续风。 */
void Board::EndTyphoonGust()
{
	mTyphoonGustActive = false;
	mActiveGustStrength = TyphoonStrength::NONE;
	mActiveGustDirection = WindDirection::NONE;
	mActiveGustDuration = 0.0f;
	mActiveGustTimer = 0.0f;
	mActiveGustPlantMoveTimer = 0.0f;
	mActiveGustPlantMoved = false;
	mWindGustTimer = mTyphoonGustsRemaining > 0
		? RandomTyphoonGustInterval(mTyphoonStrength) : 0.0f;
}

/**
 * 同一阵风按已锁定吹向逐格、从前缘到后缘结算全部植物；
 * 植物被吹出棋盘或吹入弹坑时死亡，弹坑不能反直觉地充当挡风墙。
 * 每次换格先更新 Cell、row/column 与碰撞箱，再让植物画面用瞬态偏移追赶；
 * 因此滑动中保存只会记录目标格，读档不会恢复半格状态或重复位移。
 */
void Board::TriggerTyphoonPlantMove(TyphoonStrength strength, WindDirection direction)
{
	mLastTyphoonMovedPlants = 0;
	mLastTyphoonLostPlants = 0;
	if (!HasTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| direction == WindDirection::NONE) return;

	const int columnDelta = direction == WindDirection::TOWARD_FRONT ? 1 : -1;
	const int distance = TyphoonGustDistance(strength);
	std::unordered_set<int> movedPlantIDs;
	std::unordered_set<int> lostPlantIDs;
	for (int step = 0; step < distance; ++step) {
		for (int row = 0; row < mRows; ++row) {
			const int firstColumn = columnDelta > 0 ? mColumns - 1 : 0;
			const int endColumn = columnDelta > 0 ? -1 : mColumns;
			for (int column = firstColumn; column != endColumn; column -= columnDelta) {
				Cell* source = GetCell(row, column);
				if (!source || source->IsEmpty()) continue;
				const int underID = source->GetUnderPlantID();
				const int normalID = source->GetNormalPlantID();
				Plant* under = mEntityManager.GetPlant(underID);
				Plant* normal = mEntityManager.GetPlant(normalID);
				if (!under || !under->IsActive()) {
					source->ClearUnderPlantID();
					under = nullptr;
				}
				if (!normal || !normal->IsActive()) {
					source->ClearNormalPlantID();
					normal = nullptr;
				}
				if (!under && !normal) continue;

				const int targetColumn = column + columnDelta;
				if (targetColumn < 0 || targetColumn >= mColumns) {
					if (under) {
						lostPlantIDs.insert(underID);
						under->Die();
					}
					if (normal) {
						lostPlantIDs.insert(normalID);
						normal->Die();
					}
					continue;
				}

				Cell* target = GetCell(row, targetColumn);
				if (!target || !target->IsEmpty()) continue;
				if (HasCraterAt(row, targetColumn)) {
					if (under) {
						lostPlantIDs.insert(underID);
						under->Die();
					}
					if (normal) {
						lostPlantIDs.insert(normalID);
						normal->Die();
					}
					continue;
				}
				source->ClearUnderPlantID();
				source->ClearNormalPlantID();
				if (under) {
					target->SetUnderPlantID(underID);
					under->MoveToGridCell(row, targetColumn, kTyphoonPlantSlideDuration);
					movedPlantIDs.insert(underID);
				}
				if (normal) {
					target->SetNormalPlantID(normalID);
					normal->MoveToGridCell(row, targetColumn, kTyphoonPlantSlideDuration);
					movedPlantIDs.insert(normalID);
				}
				RefreshPlantStackRenderOrder(target);
			}
		}
	}
	mLastTyphoonMovedPlants = static_cast<int>(movedPlantIDs.size());
	mLastTyphoonLostPlants = static_cast<int>(lostPlantIDs.size());
}

void Board::BeginRain(RainIntensity intensity, float duration, bool canIntensify, bool canHold,
	bool allowTyphoonRoll)
{
	if (intensity == RainIntensity::CLEAR || duration <= 0.0f) return;
	const bool wasHeavy = mRainIntensity == RainIntensity::HEAVY;
	BeginWeatherTransition(intensity);
	if (intensity != RainIntensity::HEAVY) {
		ClearPendingHeavyRainWarning();
		StopTyphoon();
	}
	else if (!wasHeavy) {
		// 小雨后续若已增强为大雨，本轮已经兑现压力，不再把它算作弱天气欠账。
		mWeakWeatherPhasesSinceHeavy = 0;
		if (allowTyphoonRoll) ConsumePendingHeavyTyphoon();
		else {
			ClearPendingHeavyRainWarning();
			StopTyphoon();
		}
	}
	else {
		ClearPendingHeavyRainWarning();
	}
	mForecastRainIntensity = RainIntensity::CLEAR;
	mActualForecastRainIntensity = RainIntensity::CLEAR;
	mWeatherTimer = duration;
	mRainCanIntensify = canIntensify && intensity == RainIntensity::LIGHT;
	mRainCanHold = canHold && intensity != RainIntensity::LIGHT;
	mWeatherForecastReady = false;
	mRainSplashTimer = RandomRainSplashDelay(intensity);
	mLightningTimer = (intensity == RainIntensity::HEAVY)
		? GameRandom::Range(kLightningDelayMin, kLightningDelayMax)
		: 0.0f;
	mRainVisualActive = false;
	RefreshZombieWeatherSpeeds();
	// 同档续期也新建发射器，让旧雨丝自然收尾并与新雨段无缝衔接。
	EmitRainEffect(duration);
	StartRainAudio();
	if (mGameScene) mGameScene->ShowCurrentWeatherNotice();
}

void Board::FinishRainPhase(int transitionRoll)
{
	const float directorFactor = GetWeatherDirectorFactor();
	const RainIntensity next = RainTransitionForRoll(
		mRainIntensity, mRainCanIntensify, mRainCanHold,
		directorFactor, transitionRoll);
	if (next == RainIntensity::CLEAR) {
		EndRain();
		return;
	}

	// 同档续期或切档后统一进入衰减链，不再拥有增强或继续维持资格。
	BeginRain(next, RandomTailRainDuration(next, directorFactor), false, false);
}

void Board::EndRain()
{
	ClearPendingHeavyRainWarning();
	StopTyphoon();
	BeginWeatherTransition(RainIntensity::CLEAR);
	mForecastRainIntensity = RainIntensity::CLEAR;
	mActualForecastRainIntensity = RainIntensity::CLEAR;
	mWeatherTimer = GameRandom::Range(kClearWeatherDelayMin, kClearWeatherDelayMax);
	mLightningTimer = 0.0f;
	mRainSplashTimer = 0.0f;
	mRainCanIntensify = false;
	mRainCanHold = false;
	mWeatherForecastReady = false;
	mRainVisualActive = false;
	mRainVisualEffectName.clear();
	RefreshZombieWeatherSpeeds();
	if (mWeatherTransitionTimer > 0.0f) StartRainAudio();
	else StopRainAudio();
	if (mGameScene) mGameScene->ShowCurrentWeatherNotice();
}

void Board::TriggerLightning()
{
	if (mRainIntensity != RainIntensity::HEAVY || !mGameScene) return;
	// 仅做短促低峰值白闪；不启用动态光源、阴影或材质高光。
	mGameScene->ShowScreenFlash(kLightningFlashDuration, kLightningFlashPeakAlpha);
}

void Board::UpdateWeather(float deltaTime)
{
	if (!mWeatherInitialized || deltaTime <= 0.0f || !SupportsWeather()) return;
	UpdateWeatherTransition(deltaTime);

	// 每帧只推进当前阶段的倒计时。雨中归零会按当前强度决定续期、增强、衰减或放晴；
	// CLEAR 阶段归零可继续晴天或进入新雨，雨链本身仍按资格形成有界循环。
	mWeatherTimer -= deltaTime;
	if (mWeatherTimer <= kWeatherForecastLeadTime && !mWeatherForecastReady) {
		PrepareWeatherForecast();
	}
	MaybeShowHeavyRainPrompt();
	if (mRainIntensity != RainIntensity::CLEAR && mWeatherTimer > 0.0f
		&& !IsRainEffectEmitting()) {
		// 粒子系统若因场景清理或异常耗尽而与天气状态脱节，用剩余时长自动补发。
		mRainVisualActive = false;
		EmitRainEffect(mWeatherTimer);
	}
	if (mRainIntensity != RainIntensity::CLEAR && mWeatherTimer > 0.0f) {
		UpdateRainGroundSplash(deltaTime);
	}
	if (mRainIntensity == RainIntensity::HEAVY && mWeatherTimer > 0.0f) {
		UpdateTyphoon(deltaTime);
		mLightningTimer -= deltaTime;
		if (mLightningTimer <= 0.0f) {
			TriggerLightning();
			mLightningTimer = GameRandom::Range(kLightningRepeatMin, kLightningRepeatMax);
		}
	}

	if (mWeatherTimer > 0.0f) return;
	ConsumeWeatherForecast();
}

void Board::SetRainForTesting(RainIntensity intensity, float duration, bool canIntensify)
{
	if (!SupportsWeather()) return;
	if (g_particleSystem) g_particleSystem->ClearAll();
	StopRainAudio();
	mWeatherInitialized = true;
	mPreviousRainIntensity = mRainIntensity;
	mWeatherTransitionTimer = 0.0f;
	ClearPendingHeavyRainWarning();
	mForecastRainIntensity = RainIntensity::CLEAR;
	mActualForecastRainIntensity = RainIntensity::CLEAR;
	mWeatherForecastReady = false;
	mRainVisualActive = false;
	mRainVisualEffectName.clear();
	mHeavyPhasesWithoutTyphoon = 0;
	StopTyphoon();

	if (intensity == RainIntensity::CLEAR) {
		mRainIntensity = RainIntensity::CLEAR;
		mPreviousRainIntensity = RainIntensity::CLEAR;
		mWeatherTimer = std::max(duration, 0.1f);
		mLightningTimer = 0.0f;
		mRainSplashTimer = 0.0f;
		mRainCanIntensify = false;
		mRainCanHold = false;
		FinishWeatherTransitionImmediately();
		if (mGameScene) mGameScene->ShowCurrentWeatherNotice();
		return;
	}
	BeginRain(intensity, std::max(duration, 0.1f), canIntensify, true, false);
	FinishWeatherTransitionImmediately();
}

bool Board::SetWeatherForecastForTesting(RainIntensity forecast, RainIntensity actual, float revealIn)
{
	if (!SupportsWeather() || !mWeatherInitialized) {
		return false;
	}
	ClearPendingHeavyRainWarning();
	mForecastRainIntensity = forecast;
	mActualForecastRainIntensity = actual;
	mWeatherForecastReady = true;
	mWeatherTimer = std::max(revealIn, 0.1f);
	PreparePendingHeavyTyphoon();
	return true;
}

/** 用确定性台风初态覆盖测试预报；NONE 同样表示已经锁定，而不是尚未抽取。 */
bool Board::SetPendingHeavyTyphoonForTesting(TyphoonStrength strength, int promptVariant)
{
	if (!mWeatherForecastReady || mActualForecastRainIntensity != RainIntensity::HEAVY
		|| mRainIntensity == RainIntensity::HEAVY
		|| strength < TyphoonStrength::NONE || strength > TyphoonStrength::SUPER
		|| promptVariant < -1 || promptVariant > 2) {
		return false;
	}
	mPendingHeavyTyphoonPrepared = true;
	mPendingHeavyTyphoonStrength = strength;
	mPendingHeavyWindDirection = strength == TyphoonStrength::NONE
		? WindDirection::NONE : WindDirection::TOWARD_HOUSE;
	mPendingHeavyTyphoonStrengthTimer = strength == TyphoonStrength::NONE ? 0.0f : 60.0f;
	mPendingHeavyWindDirectionTimer = strength == TyphoonStrength::NONE ? 0.0f : 20.0f;
	mPendingHeavyTyphoonGustsRemaining = TyphoonMaxGusts(strength);
	mPendingHeavyWindGustTimer = mPendingHeavyTyphoonGustsRemaining > 0 ? 15.0f : 0.0f;
	if (promptVariant >= 0) mPendingHeavyRainPromptVariant = promptVariant;
	mHeavyRainPromptShown = false;
	return true;
}

/** 用固定权重落点准备真实新天气；公开预报强制等于实际结果，避免测试再依赖第二次随机。 */
bool Board::PrepareWeatherForecastForTesting(int weatherRoll, float revealIn)
{
	if (!SupportsWeather()
		|| !mWeatherInitialized || mRainIntensity != RainIntensity::CLEAR
		|| mWeatherForecastReady) return false;
	const int total = GetNextWeatherRollTotal();
	if (weatherRoll < 1 || weatherRoll > total) return false;
	PrepareWeatherForecast(weatherRoll);
	mForecastRainIntensity = mActualForecastRainIntensity;
	mWeatherTimer = std::max(revealIn, 0.1f);
	return true;
}

bool Board::AdvanceRainPhaseForTesting(int transitionRoll)
{
	if (mRainIntensity == RainIntensity::CLEAR) return false;
	const float directorFactor = GetWeatherDirectorFactor();
	const int total = RainTransitionWeightTotal(
		mRainIntensity, mRainCanIntensify, mRainCanHold, directorFactor);
	if (total > 0 && (transitionRoll < 1 || transitionRoll > total)) return false;

	// 测试会在雨段尚未自然到期时强制切档；先清旧雨丝，模拟生产路径中旧发射器已到期。
	if (g_particleSystem) g_particleSystem->ClearAll();
	mRainVisualActive = false;
	mRainVisualEffectName.clear();
	if (total > 0) FinishRainPhase(transitionRoll);
	else EndRain();
	FinishWeatherTransitionImmediately();
	return true;
}

bool Board::TriggerLightningForTesting()
{
	if (mRainIntensity != RainIntensity::HEAVY) return false;
	TriggerLightning();
	return true;
}

bool Board::SetTyphoonForTesting(TyphoonStrength strength, WindDirection direction,
	float gustIn, float directionIn, int gustsRemaining, float decayIn)
{
	if (mRainIntensity != RainIntensity::HEAVY) return false;
	if (strength == TyphoonStrength::NONE) {
		StopTyphoon();
		RestartRainVisualForWindChange();
		return true;
	}
	RestoreTyphoonState(strength, direction, decayIn, gustIn, directionIn, gustsRemaining);
	RestartRainVisualForWindChange();
	return HasTyphoon();
}

bool Board::RerollWindDirectionForTesting(int directionRoll)
{
	if (!HasTyphoon() || directionRoll < 1 || directionRoll > 2) return false;
	RerollWindDirection(directionRoll);
	return true;
}

bool Board::RollTyphoonForTesting(int chanceRoll, int strengthRoll, WindDirection direction)
{
	const int totalWeight = BuildTyphoonWeights(GetWeatherDirectorFactor()).Total();
	const bool validDirection = direction == WindDirection::TOWARD_HOUSE
		|| direction == WindDirection::TOWARD_FRONT;
	if (mRainIntensity != RainIntensity::HEAVY || chanceRoll < 1 || chanceRoll > 100
		|| strengthRoll < 1 || strengthRoll > totalWeight || !validDirection) return false;
	StartTyphoonForHeavyPhase(chanceRoll, strengthRoll, direction);
	RestartRainVisualForWindChange();
	return true;
}

bool Board::TriggerTyphoonGustForTesting(float plantMoveIn)
{
	if (!HasTyphoon() || mRainIntensity != RainIntensity::HEAVY) return false;
	return BeginTyphoonGust(false, plantMoveIn);
}

bool Board::IsPoolBackground() const
{
	return mBackGround == Background::WATER_POOL
		|| mBackGround == Background::NIGHT_WATER_POOL;
}

bool Board::SupportsWeather() const
{
	if (mLevel == SURVIVAL_ENDLESS_NIGHT_LEVEL) return true;
	return AdventureProgression::IsAdventureLevel(mLevel)
		&& AdventureProgression::GetAreaNumber(mLevel) >= 2;
}

bool Board::IsPoolRow(int row) const
{
	return IsPoolBackground() && row >= kPoolFirstRow && row <= kPoolLastRow;
}

bool Board::IsPoolSquare(int row, int col) const
{
	return IsPoolRow(row) && col >= 0 && col < mColumns;
}

bool Board::IsPoolWorldPosition(int row, float x) const
{
	const float poolRight = CELL_INITALIZE_POS_X
		+ static_cast<float>(mColumns) * CELL_COLLIDER_SIZE_X;
	return IsPoolRow(row) && x >= CELL_INITALIZE_POS_X && x < poolRight;
}

/** 返回指定类型能否被正式选行逻辑放进水路；未来禁水类型只需集中追加到此。 */
bool Board::CanZombieTypeSpawnInPool(ZombieType type) const
{
	switch (type) {
	case ZombieType::NUM_ZOMBIE_TYPES:
		return false;
	default:
		return true;
	}
}

Vector Board::GetCellCenterPosition(int row, int col) const
{
	return Vector(CELL_INITALIZE_POS_X + static_cast<float>(col) * CELL_COLLIDER_SIZE_X
		+ CELL_COLLIDER_SIZE_X * 0.5f,
		mCellInitialY + static_cast<float>(row) * mCellHeight + mCellHeight * 0.5f);
}

void Board::InitializeCell(int rows, int cols)
{
	mRows = rows + 1;
	mColumns = cols + 1;
	mCellInitialY = IsPoolBackground()
		? kPoolCellInitialY
		: CELL_INITALIZE_POS_Y;
	mCellHeight = IsPoolBackground() ? kPoolCellHeight : CELL_COLLIDER_SIZE_Y;
	mCells.resize(mRows);
	for (int i = 0; i < mRows; i++)
	{
		mCells[i].resize(mColumns);
		for (int j = 0; j < mColumns; j++)
		{
			Vector position(CELL_INITALIZE_POS_X + j * CELL_COLLIDER_SIZE_X,
				mCellInitialY + i * mCellHeight);
			Cell* cell = GameObjectManager::GetInstance().CreateGameObject<Cell>(
				LAYER_BACKGROUND, i, j, position,
				Vector(CELL_COLLIDER_SIZE_X, mCellHeight));
			mCells[i][j] = cell;
		}
	}
}

void Board::CreateBoom(const Vector& position, int damage)
{
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
				// 统一灰烬入口内部决定化灰或数值扣血；特殊僵尸可拒绝化灰并限制每次灰烬伤害。
				zombie->TakePlantAshDamage(damage);
			}
		}
	}
}

void Board::CreateDoomBoom(const Vector& position, int damage)
{
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
				zombie->TakePlantAshDamage(damage);
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

bool Board::CanPlantAt(PlantType type, int row, int col)
{
	if (!HasPlantingQuota(type)) return false;

	Cell* cell = GetCell(row, col);
	if (!cell || HasCraterAt(row, col)) return false;

	const bool isWater = IsPoolSquare(row, col);
	if (type == PlantType::PLANT_LILYPAD
		|| type == PlantType::PLANT_TANGLEKELP) {
		// 水草与睡莲都是直接落水的植物；水草占普通层，因此空格判断也同时禁止叠在睡莲上。
		return isWater && cell->IsEmpty();
	}
	if (isWater) {
		// 土豆雷没有水面形态；即使已有睡莲也不能落在水路。
		if (type == PlantType::PLANT_POTATOMINE) return false;
		return cell->GetUnderPlantID() != NULL_PLANT_ID
			&& cell->GetNormalPlantID() == NULL_PLANT_ID;
	}
	return cell->GetNormalPlantID() == NULL_PLANT_ID
		&& cell->GetUnderPlantID() == NULL_PLANT_ID;
}

bool Board::HasPlantingQuota(PlantType type) const
{
	return type != PlantType::PLANT_ELITE_SCAREDYSHROOM
		|| mEliteScaredyShroomsPlanted < kEliteScaredyShroomPlantLimit;
}

int Board::GetEliteScaredyShroomPlantLimit() const
{
	return kEliteScaredyShroomPlantLimit;
}

Plant* Board::GetTopPlantAt(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) return nullptr;
	Cell* cell = mCells[row][col];
	return cell ? mEntityManager.GetPlant(cell->GetTopPlantID()) : nullptr;
}

void Board::RefreshPlantStackRenderOrder(Cell* cell)
{
	if (!cell) return;
	Plant* under = mEntityManager.GetPlant(cell->GetUnderPlantID());
	Plant* normal = mEntityManager.GetPlant(cell->GetNormalPlantID());
	if (!under || !normal) return;
	const int underOrder = under->GetRenderOrder();
	const int normalOrder = normal->GetRenderOrder();
	if (underOrder < normalOrder) return;
	under->SetRenderOrder(normalOrder);
	normal->SetRenderOrder(underOrder == normalOrder ? normalOrder + 1 : underOrder);
}

Plant* Board::CreatePlant(PlantType plantType, int row, int column, bool skipsettings, bool isPreview)
{
	// 检查行列是否有效
	if (row < 0 || row >= mRows || column < 0 || column >= mColumns) {
		LOG_ERROR("Board") << "无效的行列位置: (" << row << ", " << column << ")";
		return nullptr;
	}

	// 正式创建入口也执行累计次数闸门，覆盖 AutoTest/develop 等绕过 CanPlantAt 的调用者。
	// 读档实体恢复由已保存的累计计数约束，不能在逐株重建时重复消耗次数。
	const bool consumesPlantingQuota = !isPreview && !skipsettings && !mIsLoadSave;
	if (consumesPlantingQuota && !HasPlantingQuota(plantType)) {
		return nullptr;
	}

	// 根据植物类型创建对应的植物
	std::shared_ptr<Plant> plant = GameAPP::GetInstance().InstantiatePlant(plantType, this, row, column, isPreview);

	if (plant && !isPreview && !skipsettings) {
		Cell* cell = GetCell(row, column);
		const bool isUnderPlant = plantType == PlantType::PLANT_LILYPAD;
		const int occupiedID = isUnderPlant
			? cell->GetUnderPlantID() : cell->GetNormalPlantID();
		if (occupiedID != NULL_PLANT_ID) {
			plant->Die();
			return nullptr;
		}
		mEntityManager.AddPlant(plant);

		// 将植物与格子关联
		if (isUnderPlant) cell->SetUnderPlantID(plant->mPlantID);
		else cell->SetNormalPlantID(plant->mPlantID);
		RefreshPlantStackRenderOrder(cell);
		if (plantType == PlantType::PLANT_ELITE_SCAREDYSHROOM && consumesPlantingQuota) {
			++mEliteScaredyShroomsPlanted;
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
			if (mCells[i][j]->GetUnderPlantID() == plantID)
				mCells[i][j]->ClearUnderPlantID();
			if (mCells[i][j]->GetNormalPlantID() == plantID)
				mCells[i][j]->ClearNormalPlantID();
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
	// 普通关压力随波次推进；先刷新存活僵尸，再生成使用同一新倍率的本波僵尸。
	RefreshZombieWeatherSpeeds();
	mEliteDancersSpawnedThisWave = 0;
	mReinforcedDoorsSpawnedThisWave = 0;
	mElitePolevaultersSpawnedThisWave = 0;
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
	UpdateZombieMetrics();

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

bool Board::IsSpawnRowCompatible(ZombieType type, int row) const
{
	if (!IsPoolBackground()) return row >= 0 && row < mRows;
	if (row < 0 || row >= mRows) return false;
	if (IsPoolRow(row)) {
		// 普通/路障/铁桶会在选行后由 ResolveTerrainZombieType 换成专用泳池版本；
		// 其余允许类型保持原类型，并复用 Zombie 基类的通用入水与裁剪。
		return CanZombieTypeSpawnInPool(type);
	}

	switch (type) {
	case ZombieType::ZOMBIE_POOL_NORMAL:
	case ZombieType::ZOMBIE_POOL_CONE:
	case ZombieType::ZOMBIE_POOL_BUCKET:
		return false;
	default:
		return true;
	}
}

// 自然波次选行在静态地形兼容性之上，再应用泳池开局四波的水路保护期。
bool Board::IsNaturalWaveSpawnRowCompatible(ZombieType type, int row) const
{
	if (!IsSpawnRowCompatible(type, row)) return false;
	return !IsPoolBackground() || mCurrentWave >= kPoolFirstWaterSpawnWave || !IsPoolRow(row);
}

ZombieType Board::ResolveTerrainZombieType(ZombieType selected, int row) const
{
	if (!IsPoolRow(row)) return selected;
	switch (selected) {
	case ZombieType::ZOMBIE_NORMAL:
		return ZombieType::ZOMBIE_POOL_NORMAL;
	case ZombieType::ZOMBIE_TRAFFIC_CONE:
		return ZombieType::ZOMBIE_POOL_CONE;
	case ZombieType::ZOMBIE_BUCKET:
		return ZombieType::ZOMBIE_POOL_BUCKET;
	default:
		return selected;
	}
}

inline int Board::SelectSpawnRow(ZombieType type)
{
	if (mRowInfos.empty()) InitializeRows();

	// 第一步：根据 loseMower 计算基础权重
	float totalWeight = 0.0f;
	for (int i = 0; i < mRows; i++)
	{
		if (!IsNaturalWaveSpawnRowCompatible(type, i)) {
			mRowInfos[i].weight = 0.0f;
			continue;
		}
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
		if (!IsNaturalWaveSpawnRowCompatible(type, i)) {
			mRowInfos[i].smoothWeight = 0.0f;
			continue;
		}
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
	if (smoothTotal <= 0.0f) {
		for (int i = 0; i < mRows; ++i)
			if (IsNaturalWaveSpawnRowCompatible(type, i)) return i;
		return 0;
	}

	float randNum = GameRandom::Range(0.0f, smoothTotal);
	float cumulative = 0.0f;
	int lastCompatibleRow = 0;
	for (int i = 0; i < mRows; i++)
	{
		if (!IsNaturalWaveSpawnRowCompatible(type, i)) continue;
		lastCompatibleRow = i;
		cumulative += mRowInfos[i].smoothWeight;
		if (cumulative >= randNum) return i;
	}
	return lastCompatibleRow;
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
	int candidatesExamined = 0;
	float x = static_cast<float>(SCENE_WIDTH) + 40;

	while (remainingPoints > 0 && zombiesSpawned < MAX_ZOMBIES_PER_WAVE
		&& candidatesExamined < kWaveCandidateAttemptLimit)
	{
		++candidatesExamined;
		ZombieType selected = PickZombieType(remainingPoints);
		int cost = GameDataManager::GetInstance().GetZombieWeight(selected);
		if (cost <= 0) break;

		// 每波受限类型在源头拒绝：不选行、不扣预算，也不替换成会干扰出怪池的普通类型。
		const ZombieType actualType = ResolveWaveZombieType(selected);
		if (actualType == ZombieType::NUM_ZOMBIE_TYPES) continue;

		int row = SelectSpawnRow(actualType);
		const ZombieType terrainType = ResolveTerrainZombieType(actualType, row);

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

		auto zombie = CreateZombie(terrainType, row, x);
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

inline void Board::UpdateZombieMetrics()
{
	int64_t TotalHP = 0, CurrectWaveHP = 0;
	int hostileZombieCountForMusic = 0;
	for (auto zombieID : mEntityManager.GetAllZombieIDs())
	{
		if (auto zombie = mEntityManager.GetZombie(zombieID))
		{
			if (zombie->IsMindControlled()) continue;	// 判断是不是魅惑
			if (!zombie->IsDying() && zombie->HasHead())
			{
				++hostileZombieCountForMusic;
			}

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
	mHostileZombieCountForMusic = hostileZombieCountForMusic;
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
	mUpdateZombieMetricsTimer += DeltaTime::GetDeltaTime();
	if (mUpdateZombieMetricsTimer >= 0.5f)
	{
		mUpdateZombieMetricsTimer = 0.0f;
		UpdateZombieMetrics();
	}
	UpdateLevel();
	AudioSystem::UpdateAdaptiveMusic(DeltaTime::GetDeltaTime(), mHostileZombieCountForMusic);
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
	}
	// 雨转晴途中读档时目标枚举已经是 CLEAR，但旧雨声仍应按剩余过渡时间淡出。
	if (mRainIntensity != RainIntensity::CLEAR
		|| (mWeatherTransitionTimer > 0.0f
			&& mPreviousRainIntensity != RainIntensity::CLEAR)) {
		StartRainAudio();
	}
	RefreshZombieWeatherSpeeds();

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
	mEliteDancersSpawnedThisWave = 0;
	mReinforcedDoorsSpawnedThisWave = 0;
	mElitePolevaultersSpawnedThisWave = 0;
	RefreshZombieWeatherSpeeds();

	// 重算难度（解锁更强僵尸）+ 刷新关卡名
	BuildSurvivalSpawnList(mSurvivalRound);
	UpdateSurvivalLevelName();

	// 回到选卡：暂停波次推进
	mBoardState = BoardState::CHOOSE_CARD;

	// 通知场景：先结算两次成对词条机会（每次可选或放弃），然后再链式进入选卡。
	if (mGameScene)
		mGameScene->BeginSurvivalPerkSelect();
}

/** AutoTest 直接定位无尽轮次，并同步所有由轮次派生的天气速度状态。 */
bool Board::SetSurvivalRoundForTesting(int round)
{
	if (!mIsSurvival || round < 1) return false;
	mSurvivalRound = round;
	BuildSurvivalSpawnList(round);
	UpdateSurvivalLevelName();
	RefreshZombieWeatherSpeeds();
	return true;
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
		// 可选：每关初始阳光（如黑夜收官关 18 给 1500）。只在开新关生效——
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
	const bool isUnderPlant = type == PlantType::PLANT_LILYPAD;
	if (cell && (isUnderPlant
		? cell->GetUnderPlantID() : cell->GetNormalPlantID()) != NULL_PLANT_ID) {
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
			if (isUnderPlant) cell->SetUnderPlantID(id);
			else cell->SetNormalPlantID(id);
			RefreshPlantStackRenderOrder(cell);
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
	float y = GetCellCenterPosition(row, 0).y - 3.0f;
	const AnimationType animType = type == MowerType::WATER
		? AnimationType::ANIM_POOL_CLEANER : AnimationType::ANIM_LAWNMOWER;
	const float scale = type == MowerType::WATER ? 0.8f : 0.85f;

	auto mower = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Mower>(
		LAYER_GAME_OBJECT, this, type, animType, x, y, row, scale);

	if (mower) {
		mEntityManager.AddMower(mower);
	}
	return mower.get();
}

Mower* Board::CreateMowerWithID(MowerType type, int row, float x, float y, int id)
{
	const AnimationType animType = type == MowerType::WATER
		? AnimationType::ANIM_POOL_CLEANER : AnimationType::ANIM_LAWNMOWER;
	const float scale = type == MowerType::WATER ? 0.8f : 0.85f;
	auto mower = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Mower>(
		LAYER_GAME_OBJECT, this, type, animType, x, y, row, scale);

	if (mower) {
		mEntityManager.AddMowerWithID(mower, id);
	}
	return mower.get();
}

void Board::InitializeMowers()
{
	for (int row = 0; row < mRows; row++) {
		CreateMower(IsPoolRow(row) ? MowerType::WATER : MowerType::LAWN, row);
	}
}

/** 复制 ID 后逐一销毁其他小推车，避免 Die() 延迟回收期间修改遍历来源。 */
void Board::RemoveOtherMowersWithoutTrigger(int preservedMowerID)
{
	const std::vector<int> mowerIDs = mEntityManager.GetAllMowerIDs();
	for (int id : mowerIDs) {
		if (id == preservedMowerID) continue;
		Mower* mower = mEntityManager.GetMower(id);
		if (!mower) continue;
		SetRowLoseMower(mower->mRow);
		mower->Die();
	}
}

float Board::GetZombieCollisionY(int row) const
{
	if (row < 0 || row >= mRows) {
		LOG_ERROR("Board") << "GetZombieCollisionY: 无效的行索引: " << row;
		return -1.0f;
	}

	// 碰撞始终锚定网格逻辑行；第三大关公共地图修正仍属于整张棋盘的基线。
	const float mapAlignmentOffset = mBackGround == Background::WATER_POOL
		? kThirdAreaZombieAlignmentOffsetY
		: 0.0f;
	return GetCellCenterPosition(row, 0).y + kZombieSpawnBaseOffsetY + mapAlignmentOffset
		+ (IsPoolBackground()
			? kPoolBackgroundZombieSpawnYOffset
			: 0.0f);
}

float Board::GetZombieSpawnY(int row) const
{
	const float collisionY = GetZombieCollisionY(row);
	if (collisionY < 0.0f) return collisionY;

	// 水路身体需要继续保持主人校对过的下沉画面，但该偏移只影响生成/绘制基准。
	return collisionY + (IsPoolRow(row) ? kPoolRowZombieSpawnYOffset : 0.0f);
}

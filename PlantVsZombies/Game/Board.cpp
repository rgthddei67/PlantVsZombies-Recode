#include "Board.h"
#include "../Logger.h"
#include "BoardPresentation.h"
#include "LawnMower.h"
#include "Shovel.h"
#include "Sun.h"
#include "Trophy.h"
#include "Crater.h"
#include "Ladder.h"
#include "IceWall.h"
#include "GroundRift.h"
#include "AdventureProgression.h"
#include "AI/PlantDefenseMonteCarlo.h"
#include "CardSlotManager.h"
#include "Card.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Plant/Imitater.h"
#include "./Plant/Blover.h"
#include "./Plant/PlantUpgradeRules.h"
#include "./Plant/PlantFootprint.h"
#include "./Plant/Plantern.h"
#include "./Plant/CobCannon.h"
#include "./Zombie/Zombie.h"
#include "./Zombie/BobsledTeamZombie.h"
#include "./Zombie/HealerZombie.h"
#include "./Zombie/HijackerZombie.h"
#include "./Zombie/InsulatorZombie.h"
#include "./Zombie/IceStatueExecutionerZombie.h"
#include "./Zombie/MagneticItem.h"
#include "MistFuel.h"

#include "EntityRegistry.h"
#include "RenderOrder.h"
#include "AudioSystem.h"
#include "./Plant/GameDataManager.h"
#include "../GameApp.h"
#include "../FileManager.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../ParticleSystem/ParticleSystem.h"
#include "../Graphics.h"
#include "../Profiler.h"
#include <unordered_set>
#include <climits>
#include <array>
#include <algorithm>   // std::max, std::swap
#include <cmath>       // std::lround
#include <cstdint>
#include <chrono>
#include <limits>
#include <unordered_map>

namespace {
	/** 返回当前地形唯一的关卡音乐资源键，供预构建与正式播放共用。 */
	const std::string& BackgroundMusicKey(Background background)
	{
		switch (background)
		{
		case Background::GROUND_DAY:
			return ResourceKeys::Music::MUSIC_DAY;
		case Background::GROUND_NIGHT:
			return ResourceKeys::Music::MUSIC_NIGHT;
		case Background::WATER_POOL:
			return ResourceKeys::Music::MUSIC_POOL;
		case Background::NIGHT_WATER_POOL:
			return ResourceKeys::Music::MUSIC_FOG;
		case Background::ROOF:
			return ResourceKeys::Music::MUSIC_ROOF;
		case Background::NIGHT_ROOF:
			return ResourceKeys::Music::MUSIC_NIGHT;
		case Background::WINTER_GARDEN:
			return ResourceKeys::Music::MUSIC_DAY;
		case Background::POLAR_NIGHT_SNOWFIELD:
			return ResourceKeys::Music::MUSIC_NIGHT;
		}
		return ResourceKeys::Music::MUSIC_DAY;
	}

	/** 集中保留地刺系的背景地形规则；屋顶必须拒绝无法放入花盆的地刺系。 */
	bool IsSpikeweedTerrainRestricted(Background background)
	{
		switch (background) {
		case Background::ROOF:
		case Background::NIGHT_ROOF:
			return true;
		default:
			return false;
		}
	}

	constexpr float kPoolCellInitialY = 85.0f;            // 泳池背景共用的六行网格首行顶部世界坐标（像素）
	constexpr float kPoolCellHeight = 85.0f;              // 泳池六行的逻辑格高（像素）；列宽仍保持 80
	constexpr float kRoofCellInitialYOffsetY = -10.0f;    // 屋顶五行网格相对普通草坪首行顶部的上移量（像素）
	constexpr float kRoofCellHeight = 85.0f;              // 屋顶五行的逻辑格高（像素）；与原版屋顶行距一致
	constexpr int kRoofSlopeColumnCount = 5;              // 从房屋侧起参与斜坡抬升的逻辑列数
	constexpr float kRoofSlopeRisePerPixel = 0.25f;       // 屋顶向房屋侧每移动 1 像素增加的屏幕 Y（像素/像素）
	constexpr float kZombieSpawnBaseOffsetY = 2.0f;       // 第一、二大关已确认正确的僵尸行中心统一基线（像素）
	constexpr float kRoofZombieAlignmentOffsetY = 17.5f; // 屋顶僵尸脚底相对 85px 行中心的美术校准量，单位：像素
	constexpr float kPoolBackgroundZombieSpawnYOffset = 0.0f; // 所有泳池背景、所有行共用的僵尸额外基线，单位：像素
	constexpr float kThirdAreaZombieAlignmentOffsetY = 10.0f; // 仅第三大关所有行使用的地图对齐基线，单位：像素
	constexpr float kPoolRowZombieSpawnYOffset = 30.0f;   // 仅水路行僵尸的画面下沉量；碰撞基线不含此值，单位：像素
	constexpr int kPoolFirstRow = 2;                      // 泳池第一条水路的 0-based 行号
	constexpr int kPoolLastRow = 3;                       // 泳池最后一条水路的 0-based 行号
	constexpr int kPoolFirstWaterSpawnWave = 5;           // 泳池自然波次从第几波起允许选择水路
	constexpr float kIceTrailDuration = 40.0f;            // 冰车进入战场后每次延伸刷新冰道的寿命，单位秒
	constexpr float kGoldenIceTrailDuration = 35.0f;      // 黄色冰道每次延伸刷新的寿命，单位秒
	constexpr float kIceTrailFadeDuration = 0.1f;         // 冰道最后渐隐时长，单位秒
	constexpr float kIceTrailLeftLimit = 25.0f;           // 原版非屋顶冰道左缘最小 X，单位 px
	constexpr float kIceTrailCapBodyOverlap = 8.0f;       // 端盖与主体纹理的水平咬合量，单位 px
	constexpr float kIceTrailGridProbeOffset = 12.0f;     // 原版由冰道左缘推导首个禁种格的采样偏移，单位 px
	constexpr float kRoofMowerTerrainOffsetY = 9.0f;      // RoofCleaner 逻辑原点相对连续行中心的下移量，单位：像素
	constexpr float kRoofPreviewZombieMinX = 1160.0f;     // 主人实测的屋顶选卡预览世界坐标左缘，单位：像素
	constexpr float kRoofPreviewZombieMaxX = 1365.0f;     // 主人实测的屋顶选卡预览世界坐标右缘，单位：像素
	constexpr int kRoofPreviewZombieFirstRow = 1;         // 顶行会令僵尸头部进入天空区；预览只使用第 2～5 行
	constexpr float kRoofMarshalBossSpawnX = 1000.0f;     // 督军高血量不水平推进，必须在右侧可见纵深直接登场，单位：像素
	constexpr float kIceTrailTopOffset = 20.0f;           // 冰道相对逻辑行顶的绘制偏移，单位 px
	constexpr float kFirstRainDelayMin = 90.0f;          // 开局到首场雨的最短等待时间（秒）
	constexpr float kFirstRainDelayMax = 105.0f;          // 开局到首场雨的最长等待时间（秒）
	constexpr float kWinterWarmTemperatureC = 6.0f;      // 寒潮外冬日花园的稳定环境温度（摄氏度）
	constexpr float kWinterFreezeTemperatureC = 0.0f;    // 降水转雪并开始冻结最右列的冰点（摄氏度）
	constexpr float kWinterColdTemperatureC = -12.0f;    // 寒潮最冷阶段的稳定环境温度（摄氏度）
	constexpr float kPolarBaselineTemperatureC = -14.0f; // 极夜每轮无危险起点温度，单位摄氏度
	constexpr float kPolarBaselineHumidity = 58.0f;      // 极夜每轮无危险起点相对湿度，百分比
	constexpr float kPolarBaselineWindMps = 8.0f;        // 极夜每轮无危险起点风速，米/秒
	constexpr float kPolarTemperatureDangerC = -18.0f;   // 温度仪危险阈值，摄氏度
	constexpr float kPolarHumidityDanger = 85.0f;        // 湿度仪危险阈值，百分比
	constexpr float kPolarWindDangerMps = 18.0f;         // 风速仪危险阈值，米/秒
	constexpr float kPolarWhiteoutCommitSeconds = 5.0f;  // 三项连续危险到不可逆提交的游戏秒
	constexpr float kPolarHoleCommitSeconds = 3.0f;      // 高湿连续到雪穴选点的游戏秒
	constexpr float kPolarHoleFormationSeconds = 2.0f;   // 雪堆占位到雪穴活动态的游戏秒
	constexpr float kPolarHoleSpawnWarningSeconds = 1.0f; // 正式波次经雪穴出生前的雪雾预警，游戏秒
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
	constexpr float kPolarSnowBlindRange = 3.0f * CELL_COLLIDER_SIZE_X; // 雪盲自动索敌真实半径，像素
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
	constexpr int kStormyNightLevel = 36;                 // 暴风雨夜专属冒险关：内部 level 36 即 4-9
	constexpr int kStormyNightForecastWave = 22;          // 第 22 波开始固定发布“暴风雨”预报
	constexpr int kStormyNightStartWave = 23;             // 第 23 波正式进入暴风雨夜，持续到第 30 波
	constexpr float kStormyNightWavePointMultiplier = 2.0f; // 暴风雨夜每波僵尸生成点数倍率
	constexpr float kStormyNightNextWaveSeconds = 5.0f;   // 暴风雨夜普通出波最大间隔；血量阈值仍可提前出波
	constexpr float kStormyNightLockedDuration = 3600.0f; // 锁定雨势/雾势的安全运行时长；状态机不会递减
	constexpr float kStormFlashUnitSeconds = 1.5f;        // C# 原版 STORM_FLASH_TIME=150cs 换算的游戏秒
	constexpr float kStormFlashShortDelayMax = 4.0f;      // 原版较密闪电等待上界（游戏秒）
	constexpr float kStormFlashLongDelayMax = 7.5f;       // 原版较疏闪电等待上界（游戏秒）
	constexpr float kStormFlashDelayMin = 3.0f;           // 每轮黑屏后等待闪光的最短游戏秒
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
	constexpr float kHeavyRainPromptLeadTime = 5.0f;     // 公开预报揭晓前弹出大雨分级文字警报的提前量（游戏秒）
	constexpr float kMaximumWeatherPanelInterferenceDuration = 300.0f; // 多次整栏黑障叠加后的当前剩余时长上限（游戏秒）
	constexpr int kWeatherForecastAccuracyPercent = 75;  // 前期天气预警准确率（百分比）
	constexpr int kLateWeatherForecastAccuracyPercent = 95; // 满压力天气预警准确率上限（百分比）
	constexpr float kFirstFogWeatherDelayMin = 45.0f;    // 夜间泳池开局到首次独立雾势抽取的最短游戏秒
	constexpr float kFirstFogWeatherDelayMax = 70.0f;    // 夜间泳池开局到首次独立雾势抽取的最长游戏秒
	constexpr float kDefaultFogWeatherDurationMin = 50.0f; // 原版默认雾休整阶段的最短持续游戏秒
	constexpr float kDefaultFogWeatherDurationMax = 80.0f; // 原版默认雾休整阶段的最长持续游戏秒
	constexpr float kLateDefaultFogWeatherDurationMin = 20.0f; // 满压力后默认雾休整的最短游戏秒
	constexpr float kLateDefaultFogWeatherDurationMax = 35.0f; // 满压力后默认雾休整的最长游戏秒
	constexpr float kElevatedFogWeatherDurationMin = 35.0f; // 小雾、普通迷雾或大雾事件的最短持续游戏秒
	constexpr float kElevatedFogWeatherDurationMax = 55.0f; // 小雾、普通迷雾或大雾事件的最长持续游戏秒
	constexpr float kFogWeatherForecastLeadTime = 15.0f; // 独立雾势揭晓前展示预报的游戏秒
	constexpr int kDenseFogChancePercent = 45;           // 夜间泳池前期每次雾势抽取进入大雾的概率
	constexpr int kLateDenseFogChancePercent = 85;       // 满压力时每次雾势抽取进入大雾的概率
	constexpr int kSmallFogColumnExpansion = 1;          // 小雾相对原版雾线向房屋方向额外覆盖的棋盘列数
	constexpr int kNormalFogColumnExpansion = 2;         // 普通迷雾相对原版雾线向房屋方向额外覆盖的棋盘列数
	constexpr int kDenseFogColumnExpansion = 3;          // 大雾相对原版雾线向房屋方向额外覆盖的棋盘列数
	constexpr float kBaseFogEdgeAlpha = 200.0f;          // 小雾和普通迷雾最左边缘格的目标 alpha
	constexpr float kDenseFogEdgeAlpha = 225.0f;         // 大雾最左边缘格的目标 alpha
	constexpr float kFogInteriorAlpha = 255.0f;          // 雾区内部格的目标 alpha
	constexpr float kFogFillRate = 180.0f;               // 雾生成或回流时每游戏秒最多增加的 alpha
	constexpr float kFogClearRate = 320.0f;              // 台风驱散时每游戏秒最多减少的 alpha
	constexpr float kFogTargetingAlphaThreshold = 96.0f; // 4-2 起远程索敌仍可接受的最大逐格雾 alpha
	constexpr int kFogTargetingMarginColumns = 1;         // 植物可从当前可见边界额外看入薄雾的格数
	constexpr float kFogCloseDetectionRange = 100.0f;    // 雾中不依赖照明的近身感知横向距离（像素）
	constexpr int kMistFuelEarlyRewardAmount = 15;        // 首波单只携带者死亡时提供的雾火量
	constexpr int kMistFuelLateRewardAmount = 10;         // 最终波单只携带者死亡时提供的雾火量
	constexpr int kMistFuelCarriersPerWave = 3;           // 每波最多分配的雾火携带者数量
	constexpr float kMistFuelEarlyBaseCarrierChance = 0.50f; // 首波普通耐久僵尸加入保底累计器的基础份额
	constexpr float kMistFuelLateBaseCarrierChance = 0.25f; // 最终波普通耐久僵尸加入保底累计器的基础份额
	constexpr float kMistFuelEarlyHeavyCarrierBonus = 0.25f; // 首波高耐久僵尸相对普通僵尸最多追加的累计份额
	constexpr float kMistFuelLateHeavyCarrierBonus = 0.15f; // 最终波高耐久僵尸相对普通僵尸最多追加的累计份额
	constexpr int kPlanternLowBackRadius = 1;              // 一档向房屋侧照亮的格数
	constexpr int kPlanternLowFrontRadius = 2;             // 一档向僵尸来向照亮的格数
	constexpr int kPlanternLowVerticalRadius = 1;          // 一档向上下照亮的格数
	constexpr int kPlanternMediumBaseRadiusX = 3;          // 二档原有主体向左右照亮的格数
	constexpr int kPlanternMediumVerticalRadius = 2;       // 二档向上下照亮的格数
	constexpr int kPlanternMediumManhattanLimit = 4;       // 二档主体裁去远角时允许的最大横纵格距和
	constexpr int kPlanternMediumFrontExtension = 4;       // 二档向僵尸来向新增的最远列格距
	constexpr int kPlanternMediumFrontHalfHeight = 1;      // 二档新增前沿列向上下延伸的格数
	constexpr int kPlanternHighBaseRadiusX = 4;            // 三档原有主体向左右照亮的格数
	constexpr int kPlanternHighVerticalRadius = 3;         // 三档向上下照亮的格数
	constexpr int kPlanternHighManhattanLimit = 6;         // 三档主体裁去远角时允许的最大横纵格距和
	constexpr int kPlanternHighFrontExtension = 5;         // 三档向僵尸来向新增的最远列格距
	constexpr int kPlanternHighFrontHalfHeight = 2;        // 三档新增前沿列向上下延伸的格数
	constexpr float kPlanternHighEdgeIllumination = 0.72f; // 三档最外圈保留的照明比例
	constexpr float kSuperFogDispersalRate = 0.28f;      // 超强台风每游戏秒累积的雾驱散比例
	constexpr float kFogReturnRate = 0.06f;              // 停风后基础雾每游戏秒恢复的驱散比例
	constexpr float kFogMaximumDriftX = 180.0f;          // 持续台风把雾团推向当前风向的最大水平像素
	constexpr float kFogDriftSpeed = 240.0f;             // 雾团追随风向或回到原位的最大像素/游戏秒
	constexpr float kFogTileLeftOverlap = 15.0f;         // 雾格相对逻辑列左缘向左覆盖的像素
	constexpr float kFogTileTopOverlap = 65.0f;          // 雾格相对首行逻辑顶缘向上覆盖的像素
	constexpr int kEliteDancerMutationChancePercent = 50; // 台风以上普通舞王变异为精英舞王的概率（百分比）
	constexpr int kEliteDancerMaxPerWave = 2;             // 每波最多允许生成的精英舞王数量；超额候选直接跳过
	constexpr int kReinforcedDoorMaxPerWave = 2;          // 每波最多正式生成的加固铁门数量；超额候选直接跳过
	constexpr int kElitePolevaulterMaxPerWave = 2;        // 每波最多正式生成的精英撑杆数量；超额候选直接跳过
	constexpr int kGildedZamboniMaxPerWave = 1;           // 每波最多正式生成的鎏金冰车数量；超额候选直接跳过
	constexpr int kEliteDolphinRiderMaxPerWave = 1;       // 每波最多正式生成的精英海豚数量；超额候选直接跳过
	constexpr int kEliteJackInTheBoxMaxPerWave = 2;       // 每波最多正式生成的精英小丑数量；超额候选直接跳过
	constexpr int kEliteDiggerMaxPerWave = 1;             // 每波最多正式生成的爆破工头数量；超额候选直接跳过
	constexpr int kElitePogoMaxPerWave = 1;               // 每波最多正式生成的精英跳跳数量；超额候选直接跳过
	constexpr int kEliteLadderMaxPerWave = 1;             // 每波最多正式生成的精英扶梯数量；超额候选直接跳过
	constexpr int kEliteCatapultMaxPerWave = 1;           // 每波最多正式生成的导流投篮车数量；超额候选直接跳过
	constexpr int kRedeyeGargantuarMaxPerAdventureWave = 3; // 冒险每波最多正式生成的红眼巨人数量；无尽不启用此上限
	constexpr int kInsulatorMaxPerWave = 2;               // 每个正式波次最多成功生成的绝缘僵尸数量
	constexpr int kHijackerMaxPerWave = 2;                // 每个正式波次最多成功生成的劫持者数量
	constexpr int kHijackerSpawnCooldownWaves = 2;        // 成功处决后封锁的后续完整正式波次数
	constexpr int kGroundingZombieMaxPerWave = 2;         // 每个正式波次最多成功生成的接地僵尸数量
	constexpr int kBobsledTeamMaxPerWave = 3;             // 一次候选四人同乘；正式波次最多接纳三队
	constexpr int kIceWallEngineerMaxPerWave = 1;         // 每个正式波次最多成功生成一只冰墙工程师
	constexpr int kIceCrackDrillMaxPerWave = 3;           // 每个正式波次最多成功生成三只冰裂钻机
	constexpr int kWeatherJammerMaxPerWave = 2;           // 每个正式波次最多成功生成两只气象干扰僵尸
	constexpr int kIceStatueExecutionerMaxPerWave = 5;   // 每个正式波次最多成功生成五只冰像处刑者
	constexpr int kSnowBurrowTutorialMaxPerWave = 1;      // 8-1 每波与同时最多一只潜雪僵尸
	constexpr int kSnowBurrowCompositeMaxPerWave = 2;     // 8-2 起每波与同时最多两只潜雪僵尸
	constexpr int kSnowBurrowTutorialLevel = 64;           // 内部 64 即 8-1，雪穴形成后保底一次潜雪出生
	constexpr int kAdaptiveHelmetTutorialLevel = 66;      // 内部 66 即 8-3，首轮白毛风结束后独立教学
	constexpr int kAdaptiveHelmetMaxPerWave = 4;          // 8-3 起每波最多四只，不设同时或累计上限
	constexpr int kHijackerTutorialLevel = 49;             // 内部 49 即 6-4，使用第七波固定单体教学
	constexpr int kHijackerTutorialWave = 7;               // 6-4 首次登场的固定教学波
	constexpr int kHealerTutorialLevel = 51;               // 内部 51 即 6-6，使用第三波额外保底
	constexpr int kHealerTutorialWave = 3;                 // 6-6 首次登场的额外保底波次
	constexpr int kWeatherJammerTutorialLevel = 60;        // 内部 60 即 7-6，使用第三波额外保底
	constexpr int kWeatherJammerTutorialWave = 3;          // 7-6 首次登场的额外保底波次
	constexpr int kIceExecutionerTutorialLevel = 61;       // 7-7 首次教学冰像处刑者的冒险关卡
	constexpr int kIceExecutionerTutorialWave = 3;         // 7-7 第三波额外保底一只处刑者
	constexpr int kEliteScaredyShroomPlantLimit = 4;      // 每个关卡累计最多种植的精英胆小菇数量
	constexpr int kPumpkinProtectionCellRadius = 1;       // 南瓜头范围爆炸保护的逻辑格半径；1 表示自身九宫格
	constexpr int kPumpkinAreaDamageMultiplier = 5;       // 特殊僵尸范围伤害被南瓜头拦截时的默认基础伤害倍率
	constexpr int kPlantTargetMonteCarloRolloutCount = 48; // 蹦极与精英小丑长时域选点的每候选未来样本数
	constexpr int kTreatmentMonteCarloRolloutCount = 40; // 急救员短时域选疗的每候选未来样本数
	constexpr int kMonteCarloMaxZombies = 16;             // 单个样本最多推进的当前敌方僵尸数
	constexpr int kMonteCarloHealerDecisionSpacingSteps = 3; // 两次急救员推演至少间隔的固定逻辑步数
	constexpr float kMonteCarloHorizonSeconds = 16.0f;    // 植物防线短视推演时域，单位：游戏秒
	constexpr float kMonteCarloStepSeconds = 0.25f;       // 纯数值推演固定步长，单位：游戏秒
	constexpr float kMonteCarloPlantDecisionSeconds = 2.0f; // 样本内玩家尝试从实际卡槽种植的间隔秒数
	constexpr float kMonteCarloBacklineMultiplier = 1.2f; // 当前后半场植物的战略价值倍率
	constexpr float kMonteCarloSunProducerFutureValue = 300.0f; // 当前产能植物的未来经济价值，单位：阳光分
	constexpr float kMonteCarloTerminalBlockedSecondUtility = 12.0f; // 终局每秒剩余破墙时间对应的防守效用分
	constexpr float kMonteCarloTerminalBlockedSecondsCap = 90.0f; // 单株终局破墙时间最多计入的秒数
	constexpr float kTreatmentMonteCarloHorizonSeconds = 7.0f; // 急救员从当前选择推演到下一次最早决策的窗口，单位：游戏秒
	constexpr int kNightRoofRouteMonteCarloRolloutCount = 32; // 满雷路线每个候选共享的短视未来样本数
	constexpr float kNightRoofRouteMonteCarloHorizonSeconds = 10.0f; // 满雷路线从预警起推演的游戏秒数
	constexpr float kGroundingZombieControlImmunityDuration = 30.0f; // 成功引雷后范围免控的游戏秒数
	constexpr float kGroundingZombieControlImmunityRadius = 130.0f; // 成功引雷后减速/冻结/黄油免疫的圆形半径，单位：像素
	constexpr ZombieControlMask kGroundingZombieControlImmunityMask =
		ZombieControlBit(ZombieControlEffect::SLOW)
		| ZombieControlBit(ZombieControlEffect::FROZEN)
		| ZombieControlBit(ZombieControlEffect::BUTTER); // 接地编队免疫三类植物控制，保留麻痹和伤害反制
	constexpr float kTreatmentTerminalPressurePerHealth = 0.08f; // 时域末端每点僵尸生命折算的基础进攻压力
	constexpr int kWaveCandidateAttemptLimit = MAX_ZOMBIES_PER_WAVE * 10; // 单波候选尝试上限，防止仅剩受限类型时死循环
	constexpr float kWeatherTransitionDuration = 2.0f;   // 雨势切换时倍率、暗幕与雨声音量的平滑过渡时长（游戏秒）
	constexpr float kLateWeatherRampStart = 0.40f;       // 普通关波次进度超过该比例后开始增强后期天气（0～1）
	constexpr float kAdventurePressureFullProgress = 0.7f; // 普通关到该波次进度时达到完整天气压力（0～1）
	constexpr int kOpeningTyphoonFirstEligibleWave = 6;  // 默认开局保护结束波；进入第 6 波即可按正式规则抽取台风
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
	constexpr float kLightOverlayAlpha = 45.0f;          // 小雨世界蓝灰暗幕透明度（0～255）
	constexpr float kMediumOverlayAlpha = 70.0f;         // 中雨世界蓝灰暗幕透明度（0～255）
	constexpr float kHeavyOverlayAlpha = 120.0f;          // 大雨世界蓝灰暗幕透明度（0～255）
	constexpr float kLightRainVolume = 0.30f;            // 小雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kMediumRainVolume = 0.40f;           // 中雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kHeavyRainVolume = 0.60f;            // 大雨循环音效基础音量（0～1，仍受全局音量控制）
	constexpr float kLightSplashDelayMin = 0.08f;         // 小雨两次地面水花的最短间隔（秒，约每秒 5 次）
	constexpr float kLightSplashDelayMax = 0.09f;         // 小雨两次地面水花的最长间隔（秒，约每秒 5 次）
	constexpr float kMediumSplashDelayMin = 0.05f;        // 中雨两次地面水花的最短间隔（秒，约每秒 10 次）
	constexpr float kMediumSplashDelayMax = 0.07f;        // 中雨两次地面水花的最长间隔（秒，约每秒 10 次）
	constexpr float kHeavySplashDelayMin = 0.01f;         // 大雨两次地面水花的最短间隔（秒，约每秒 20多 次）
	constexpr float kHeavySplashDelayMax = 0.02f;         // 大雨两次地面水花的最长间隔（秒，约每秒 20多 次）
	constexpr float kRainSplashEdgePadding = 18.0f;       // 水花中心距草地网格边缘的安全距离（像素）
	constexpr float kRoofRunoffChargeMaximum = 100.0f;    // 坡面径流进入锁行预警所需的满积累值
	constexpr float kRoofRunoffLightChargePerSecond = 1.00f; // 小雨每游戏秒增加的径流积累点数
	constexpr float kRoofRunoffMediumChargePerSecond = 2.00f; // 中雨每游戏秒增加的径流积累点数
	constexpr float kRoofRunoffHeavyChargePerSecond = 3.50f; // 大雨每游戏秒增加的径流积累点数
	constexpr float kRoofRunoffClearDrainPerSecond = 0.30f; // 晴天每游戏秒自然排走的径流积累点数
	constexpr float kRoofRunoffWarningDuration = 3.0f;    // 满积累后锁行预警的持续游戏秒
	constexpr float kRoofRunoffFlowDuration = 2.20f;      // 单次目标行冲刷的持续游戏秒
	constexpr float kRoofRunoffZombieDriftSpeed = -60.0f; // 地面僵尸顺坡冲向屋檐的附加速度，像素/游戏秒
	constexpr int kRoofRunoffRetainedChargeMin = 30;      // 冲刷结束后保留湿度的均匀随机下限（百分比）
	constexpr int kRoofRunoffRetainedChargeMax = 60;      // 冲刷结束后保留湿度的均匀随机上限（百分比）
	constexpr int kRoofRunoffOneRowWeight = 50;           // 满积累事件只冲刷一行的相对权重
	constexpr int kRoofRunoffTwoRowWeight = 35;           // 满积累事件同时冲刷两行的相对权重
	constexpr int kRoofRunoffThreeRowWeight = 15;         // 满积累事件同时冲刷三行的相对权重
	constexpr float kNightRoofChargeMaximum = 100.0f;     // 黑夜屋顶进入导电瓦路预警所需的满电值
	constexpr float kNightRoofChargeLightPerSecond = 1.0f; // 小雨每游戏秒增加的黑夜屋顶电荷点数
	constexpr float kNightRoofChargeMediumPerSecond = 2.0f; // 中雨每游戏秒增加的黑夜屋顶电荷点数
	constexpr float kNightRoofChargeHeavyPerSecond = 3.0f; // 大雨每游戏秒增加的黑夜屋顶电荷点数
	constexpr float kNightRoofChargeClearLeakPerSecond = 0.5f; // 晴夜每游戏秒自然泄漏的黑夜屋顶电荷点数
	constexpr float kNightRoofChargeLightningBonus = 18.0f; // 现有大雨闪电为黑夜屋顶一次注入的电荷点数
	constexpr float kNightRoofChargeWarningDuration = 4.0f; // 满电锁定导电瓦路后的预警游戏秒数
	constexpr float kNightRoofHijackerLockThreshold = 75.0f; // 每轮首次跨过此雷荷百分比时，从现存劫持者中锁定一次
	constexpr float kNightRoofHijackerWarningDuration = 7.0f; // 满电且锁定仍有效时的全局预警游戏秒数
	constexpr float kNightRoofHijackerFinalDuration = 1.0f; // 处决前停止移动和啃食、播放专属动画的游戏秒数
	constexpr int kNightRoofHijackerSurvivalLineCap = 1200; // 生存模式处决线封顶，避免后期超高血量失去反制空间
	constexpr float kNightRoofHijackerExecuteVolume = 0.55f; // 全场处决提交时的短促紫电碎裂声音量
	constexpr float kNightRoofHijackerRainChargeBonusPerSecond = 4.1f; // 场上至少一只有效劫持者时，雨中每秒额外增加的雷荷；多只不叠加
	constexpr float kNightRoofChargeDischargeDuration = 0.65f; // 基础坡面放电的可见游戏秒数
	constexpr float kNightRoofOverchargeMaximum = 25.0f;   // 满电后最多截留给下一轮的余电点数
	constexpr float kNightRoofPlantShutdownDuration = 8.0f; // 普通瓦面植物在放电快照中停机的游戏秒数
	constexpr float kNightRoofWetPlantShutdownDuration = 20.0f; // 正在冲刷的湿坡面植物强导电停机秒数
	constexpr int kNightRoofZombieDamage = 200;             // 普通瓦面地面僵尸承受的环境伤害
	constexpr int kNightRoofWetZombieDamage = 600;         // 正在冲刷的湿坡面地面僵尸承受的强导电伤害
	constexpr float kNightRoofZombieParalysisDuration = 1.5f; // 普通瓦面非车辆僵尸的麻痹游戏秒数
	constexpr float kNightRoofWetZombieParalysisDuration = 5.5f; // 正在冲刷的湿坡面非车辆僵尸麻痹游戏秒数
	constexpr float kLightningDelayMin = 3.5f;           // 大雨开始后首次闪电的最短等待时间（秒）
	constexpr float kLightningDelayMax = 7.0f;           // 大雨开始后首次闪电的最长等待时间（秒）
	constexpr float kLightningRepeatMin = 5.0f;          // 大雨中两次闪电的最短间隔（秒）
	constexpr float kLightningRepeatMax = 10.0f;         // 大雨中两次闪电的最长间隔（秒）
	constexpr float kLightningFlashDuration = 0.42f;     // 单次闪电主放电与回闪的总可见时间（秒）
	constexpr float kThunderSoundVolume = 0.75f;         // 闪电出现时雷声相对音效音量
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
	constexpr int kSevereMaxGusts = 1;                   // 强台风单个大雨阶段最多阵风次数
	constexpr int kSuperMaxGusts = 2;                    // 超强台风单个大雨阶段最多阵风次数
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

	/** 返回雨势、低温形态与实时风向共同决定的降水特效名。 */
	const char* PrecipitationEffectName(RainIntensity intensity,
		WindDirection direction, bool snow)
	{
		if (snow) {
			switch (intensity) {
			case RainIntensity::LIGHT:  return "SnowLight";
			case RainIntensity::MEDIUM: return "SnowMedium";
			case RainIntensity::HEAVY:  return "SnowHeavy";
			case RainIntensity::CLEAR:  return "";
			}
		}
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

Board::Board(BoardPresentation* presentation, Background background, int level)
{
	mPresentation = presentation;
	mLevel = level;
	mBackGround = background;
	mIsSurvival = IsSurvivalEndlessLevel(level);
	mPlantDamageEchoHitCounter = 0; // 每次进入新棋盘都清掉旧局的十连击进度，读档随后按存档覆盖。

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

	InitializeCell(IsPoolBackground() ? 5 : 4, 8);
	// 屋顶预览会读取行高与连续坡面；必须在网格尺寸完成初始化后再生成。
	CreatePreviewZombies();
	mIceMinX.fill(GetIceTrailRightX());
	mIceTimer.fill(0.0f);
	mGoldenIceMinX.fill(GetIceTrailRightX());
	mGoldenIceTimer.fill(0.0f);
	InitializeRows();
}

Board::~Board()
{
	// 原版 Board.DisposeBoard 无条件 StopFoley(Rain)；离开关卡时同样保证循环声不泄漏到菜单。
	StopRainAudio();
}

/**
 * 结束读档生命周期，并按已恢复的雾势、驱散量和路灯花状态直接建立首帧迷雾。
 * 正常开局与后续天气变化仍走逐帧平滑，只有重进存档跳过从透明开始的暴露窗口。
 */
void Board::CompleteLoadRestore()
{
	mIsLoadSave = false;
	if (mFogWeatherInitialized && SupportsStageFog()) {
		UpdateFogCellAlpha(0.0f, true);
	}
}

float Board::GetZombieRainSpeedMultiplier() const
{
	const float progress = GetWeatherTransitionProgress();
	const float pressure = GetWeatherPressureFactor();
	const float previous = ZombieSpeedForRain(mPreviousRainIntensity, pressure);
	return previous + (ZombieSpeedForRain(mRainIntensity, pressure) - previous) * progress;
}

/** 返回开局台风保护是否正在约束当前 Board；生存模式只保护第一轮。 */
bool Board::IsOpeningTyphoonProtectionActive() const
{
	if (!GameAPP::GetInstance().mOpeningTyphoonProtectionEnabled || !SupportsTyphoon()) {
		return false;
	}
	const bool isOpeningRound = !mIsSurvival || mSurvivalRound <= 1;
	return isOpeningRound && mCurrentWave < kOpeningTyphoonFirstEligibleWave;
}

/** 当前新大雨若进行台风判定时的实际概率，包含开局保护、波次成长与连续落空保底。 */
int Board::GetCurrentTyphoonChancePercent() const
{
	if (!SupportsTyphoon()) return 0;
	if (IsOpeningTyphoonProtectionActive()) return 0;
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

/**
 * 只暂停锁定行坡段的花盆上层，不碰承载花盆本体、平台植物或其他行。
 * 由格子层 ID 判定 normal/pumpkin，避免临时覆盖层被误当作常规作物。
 */
bool Board::IsPlantPausedByRoofRunoff(const Plant* plant) const
{
	if (!plant || !plant->IsActive() || !IsRoofRunoffFlowing()
		|| !IsRoofRunoffRowSelected(plant->mRow) || plant->mColumn < 0
		|| plant->mColumn >= kRoofSlopeColumnCount
		|| plant->IsRoofSupportPlant()) return false;
	if (plant->mRow < 0 || plant->mRow >= mRows
		|| plant->mColumn >= mColumns) return false;

	Cell* cell = mCells[plant->mRow][plant->mColumn];
	if (!cell || (cell->GetNormalPlantID() != plant->mPlantID
		&& cell->GetPumpkinPlantID() != plant->mPlantID)) return false;
	Plant* support = mEntityRegistry.GetPlant(cell->GetUnderPlantID());
	return support && support->IsActive() && !support->IsSquished()
		&& support->IsRoofSupportPlant();
}

float Board::GetRainOverlayAlpha() const
{
	const float progress = GetWeatherTransitionProgress();
	const float previous = OverlayAlphaForRain(mPreviousRainIntensity);
	return previous + (OverlayAlphaForRain(mRainIntensity) - previous) * progress;
}

bool Board::IsStormyNightForecastActive() const
{
	return mLevel == kStormyNightLevel && mCurrentWave == kStormyNightForecastWave;
}

bool Board::IsStormyNightActive() const
{
	return mLevel == kStormyNightLevel && mCurrentWave >= kStormyNightStartWave;
}

bool Board::IsStormyNightFlashOn() const
{
	if (!IsStormyNightActive()) return false;
	switch (mStormyNightFlashPattern) {
	case 1:
	case 2:
		return mStormyNightFlashTimer < kStormFlashUnitSeconds * 2.0f;
	case 3:
		return mStormyNightFlashTimer < kStormFlashUnitSeconds;
	default:
		return false;
	}
}

/**
 * 复刻 C# `DrawStormFlash` 的两层线性幕：闪光开始时先降低黑幕并叠白光，随后恢复全黑。
 * pattern 1 是强弱双闪，pattern 2 是三秒长闪，pattern 3 是一点五秒短闪。
 */
void Board::GetStormyNightOverlayAlphas(float& blackAlpha, float& whiteAlpha) const
{
	blackAlpha = 0.0f;
	whiteAlpha = 0.0f;
	if (!IsStormyNightActive()) return;
	blackAlpha = 255.0f;

	float flashTime = -1.0f;
	float maxAmount = 255.0f;
	switch (mStormyNightFlashPattern) {
	case 1:
		if (mStormyNightFlashTimer < kStormFlashUnitSeconds * 2.0f) {
			if (mStormyNightFlashTimer > kStormFlashUnitSeconds) {
				flashTime = mStormyNightFlashTimer - kStormFlashUnitSeconds;
			}
			else {
				flashTime = mStormyNightFlashTimer;
				maxAmount = 92.0f;
			}
		}
		break;
	case 2:
		if (mStormyNightFlashTimer < kStormFlashUnitSeconds * 2.0f) {
			flashTime = mStormyNightFlashTimer * 0.5f;
		}
		break;
	case 3:
		if (mStormyNightFlashTimer < kStormFlashUnitSeconds) {
			flashTime = mStormyNightFlashTimer;
		}
		break;
	default:
		break;
	}
	if (flashTime < 0.0f) return;

	flashTime = std::clamp(flashTime, 0.0f, kStormFlashUnitSeconds);
	blackAlpha = 255.0f - maxAmount * (flashTime / kStormFlashUnitSeconds);
	const float whiteStart = kStormFlashUnitSeconds * 0.5f;
	if (flashTime > whiteStart) {
		whiteAlpha = maxAmount * (flashTime - whiteStart)
			/ (kStormFlashUnitSeconds - whiteStart);
	}

	// 原版每六个计数抖动一次黑幕 alpha；这里用 Board 游戏帧哈希复刻闪烁且不消费出怪 RNG。
	std::uint32_t flickerState = static_cast<std::uint32_t>(mBoardFrame / 4);
	flickerState = flickerState * 1664525u + 1013904223u;
	const int flicker = static_cast<int>((flickerState >> 24) & 63u) - 32;
	blackAlpha = std::clamp(blackAlpha + static_cast<float>(flicker), 0.0f, 255.0f);
}

float Board::GetStormyNightBlackAlpha() const
{
	float blackAlpha = 0.0f;
	float whiteAlpha = 0.0f;
	GetStormyNightOverlayAlphas(blackAlpha, whiteAlpha);
	return blackAlpha;
}

float Board::GetStormyNightWhiteAlpha() const
{
	float blackAlpha = 0.0f;
	float whiteAlpha = 0.0f;
	GetStormyNightOverlayAlphas(blackAlpha, whiteAlpha);
	return whiteAlpha;
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

/** 大雾概率复用天气导演压力，但保持独立抽取，不改写雨势权重与保底。 */
int Board::GetDenseFogChancePercent() const
{
	if (!SupportsFogWeather()) return 0;
	return LerpWeatherWeight(kDenseFogChancePercent,
		kLateDenseFogChancePercent, GetWeatherDirectorFactor());
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
	if (SupportsWinterTemperature()
		&& mAmbientTemperatureC <= kWinterFreezeTemperatureC) return 0.0f;
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

/** 第 23 波只执行一次：锁定大雨、大雾和强台风，并沿用强台风原有的一次阵风额度。 */
void Board::ActivateStormyNight()
{
	if (!IsStormyNightActive()) return;
	mStormyNightInitialized = true;
	// C# 4-10 入场从 StormFlash2 的中段开始；此后再进入随机三节奏循环。
	mStormyNightFlashPattern = 2;
	mStormyNightFlashTimer = kStormFlashUnitSeconds;
	mZombieCountDown = std::min(mZombieCountDown, kStormyNightNextWaveSeconds);

	BeginRain(RainIntensity::HEAVY, kStormyNightLockedDuration, false, false, false);
	// 暴风雨夜与第 23 波同步骤然降临，玩法倍率不沿用普通天气的两秒渐变。
	FinishWeatherTransitionImmediately();
	mLightningTimer = 0.0f;
	mWeakWeatherPhasesSinceHeavy = 0;
	mHeavyPhasesWithoutTyphoon = 0;

	mFogWeatherInitialized = true;
	mFogDispersal = 0.0f;
	mFogVisualOffsetX = 0.0f;
	BeginFogWeather(FogWeatherIntensity::DENSE, kStormyNightLockedDuration);

	StopTyphoon();
	mTyphoonStrength = TyphoonStrength::SEVERE;
	mWindDirection = WindDirectionForRoll(0);
	mTyphoonStrengthTimer = kStormyNightLockedDuration;
	mWindDirectionTimer = GameRandom::Range(
		kWindDirectionDurationMin, kWindDirectionDurationMax);
	mTyphoonGustsRemaining = kSevereMaxGusts;
	mWindGustTimer = RandomTyphoonGustInterval(mTyphoonStrength);
	mWindParticleTimer = 0.0f;
	RefreshZombieWeatherSpeeds();
	RestartRainVisualForWindChange();
}

/**
 * 暴风雨锁定由波次派生；读入旧档时补做一次初始化，新档则保留已消费的阵风与闪光计时。
 * 正常逐帧只修正可能被损坏档或测试入口破坏的组合，不会返还一次性阵风。
 */
void Board::EnforceStormyNightWeather()
{
	if (!IsStormyNightActive()) return;
	if (!mStormyNightInitialized) {
		ActivateStormyNight();
		return;
	}

	mZombieCountDown = std::min(mZombieCountDown, kStormyNightNextWaveSeconds);
	mWeatherInitialized = true;
	if (mRainIntensity != RainIntensity::HEAVY) {
		BeginRain(RainIntensity::HEAVY, kStormyNightLockedDuration, false, false, false);
		FinishWeatherTransitionImmediately();
	}
	mWeatherTimer = kStormyNightLockedDuration;
	mRainCanIntensify = false;
	mRainCanHold = false;
	mForecastRainIntensity = RainIntensity::CLEAR;
	mActualForecastRainIntensity = RainIntensity::CLEAR;
	mWeatherForecastReady = false;
	mWeatherForecastDisrupted = false;
	mLightningTimer = 0.0f;
	ClearPendingHeavyRainWarning();

	mFogWeatherInitialized = true;
	if (mFogWeatherIntensity != FogWeatherIntensity::DENSE) {
		BeginFogWeather(FogWeatherIntensity::DENSE, kStormyNightLockedDuration);
	}
	mFogWeatherTimer = kStormyNightLockedDuration;
	ClearFogWeatherForecast();

	if (mTyphoonStrength != TyphoonStrength::SEVERE) {
		const WindDirection preservedDirection = mWindDirection;
		StopTyphoon();
		mTyphoonStrength = TyphoonStrength::SEVERE;
		mWindDirection = (preservedDirection == WindDirection::TOWARD_HOUSE
			|| preservedDirection == WindDirection::TOWARD_FRONT)
			? preservedDirection : WindDirectionForRoll(0);
		mWindDirectionTimer = GameRandom::Range(
			kWindDirectionDurationMin, kWindDirectionDurationMax);
		// 已初始化后的异常修复按阵风已消费处理，禁止天气切换或读档返还额度。
		mTyphoonGustsRemaining = 0;
		mWindGustTimer = 0.0f;
		mWindParticleTimer = 0.0f;
		RestartRainVisualForWindChange();
	}
	mTyphoonStrengthTimer = kStormyNightLockedDuration;
	if (mStormyNightFlashPattern < 1 || mStormyNightFlashPattern > 3
		|| mStormyNightFlashTimer <= 0.0f) {
		ScheduleNextStormyNightFlash();
	}
}

/** 按原版 4.5～9 秒范围安排下一种闪光节奏；随机结果和剩余时间都会进入关卡存档。 */
void Board::ScheduleNextStormyNightFlash()
{
	mStormyNightFlashPattern = GameRandom::Range(1, 3);
	const float maximumDelay = GameRandom::Range(0, 1) == 0
		? kStormFlashLongDelayMax : kStormFlashShortDelayMax;
	mStormyNightFlashTimer = kStormFlashUnitSeconds
		+ GameRandom::Range(kStormFlashDelayMin, maximumDelay);
}

/** 推进 C# 三种闪光节奏，并在主闪与 pattern 1 的回闪节点分别播放雷声。 */
void Board::UpdateStormyNightFlash(float deltaTime)
{
	if (!IsStormyNightActive() || deltaTime <= 0.0f) return;
	if (mStormyNightFlashTimer <= 0.0f) ScheduleNextStormyNightFlash();
	const float previous = mStormyNightFlashTimer;
	mStormyNightFlashTimer = std::max(0.0f, mStormyNightFlashTimer - deltaTime);
	const auto crossed = [previous, this](float threshold) {
		return previous > threshold && mStormyNightFlashTimer <= threshold;
	};

	bool playThunder = false;
	if (mStormyNightFlashPattern == 1) {
		playThunder = crossed(kStormFlashUnitSeconds * 2.0f)
			|| crossed(kStormFlashUnitSeconds);
	}
	else if (mStormyNightFlashPattern == 2) {
		playThunder = crossed(kStormFlashUnitSeconds * 2.0f);
	}
	else if (mStormyNightFlashPattern == 3) {
		playThunder = crossed(kStormFlashUnitSeconds);
	}
	if (playThunder) {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_THUNDER, kThunderSoundVolume);
	}
	if (mStormyNightFlashTimer <= 0.0f) ScheduleNextStormyNightFlash();
}

void Board::InitializeWeather()
{
	if (mWeatherInitialized) return;
	// 主进度由“当前雨势 + 一个复用倒计时”驱动：CLEAR 时倒计时代表距首场雨/下一场雨，
	// 下雨时则代表当前雨段剩余时间；另用布尔值记录初始小雨是否还拥有一次增强资格。
	// 第一大关把倒计时保持为 0；冒险第二大关起及三种生存地图均启用天气。
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
	mWeatherForecastDisrupted = false;
	mWeatherPanelInterferenceTimer = 0.0f;
	ClearPendingHeavyRainWarning();
	mRainVisualActive = false;
	mRainVisualEffectName.clear();
	mWeakWeatherPhasesSinceHeavy = 0;
	mHeavyPhasesWithoutTyphoon = 0;
	mRoofRunoffCharge = 0.0f;
	mRoofRunoffRetainedCharge = 0.0f;
	mRoofRunoffPhase = RoofRunoffPhase::IDLE;
	mRoofRunoffPhaseTimer = 0.0f;
	mRoofRunoffRowMask = 0;
	mNightRoofCharge = 0.0f;
	mNightRoofOvercharge = 0.0f;
	mNightRoofChargePhase = NightRoofChargePhase::CHARGING;
	mNightRoofChargePhaseTimer = 0.0f;
	mNightRoofChargeRow = -1;
	mNightRoofChargeGuided = false;
	mNightRoofChargeGuideID = NULL_ZOMBIE_ID;
	mNightRoofChargeRouteUsedMonteCarlo = false;
	mNightRoofChargeRouteStats = {};
	mNightRoofChargeRouteDecisionMicros = 0;
	mNightRoofHijackerSelectionAttempted = false;
	mNightRoofHijackerID = NULL_ZOMBIE_ID;
	mNightRoofHijackerWarningExtended = false;
	mNightRoofHijackerFinalizing = false;
	StopTyphoon();
	mWeatherTimer = SupportsWeather()
		? GameRandom::Range(kFirstRainDelayMin, kFirstRainDelayMax)
		: 0.0f;
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

/** 初始化当前迷雾关卡的独立雾势；基础雾线仍可由关内进度调节。 */
void Board::InitializeFogWeather()
{
	const bool supportsFog = SupportsStageFog();
	const int fogCellCount = supportsFog ? mColumns * (mRows + 1) : 0;
	if (static_cast<int>(mFogCellAlpha.size()) != fogCellCount) {
		mFogCellAlpha.assign(fogCellCount, 0.0f);
	}
	if (!supportsFog) {
		mFogWeatherInitialized = false;
		mFogWeatherIntensity = FogWeatherIntensity::DEFAULT;
		mFogWeatherTimer = 0.0f;
		mFogDispersal = 0.0f;
		mFogVisualOffsetX = 0.0f;
		mFogAnimationTime = 0.0f;
		ClearFogWeatherForecast();
		return;
	}
	if (mFogWeatherInitialized) return;

	mFogWeatherInitialized = true;
	mFogWeatherIntensity = FogWeatherIntensity::DEFAULT;
	mFogWeatherTimer = GameRandom::Range(
		kFirstFogWeatherDelayMin, kFirstFogWeatherDelayMax);
	mFogDispersal = 0.0f;
	mFogVisualOffsetX = 0.0f;
	mFogAnimationTime = 0.0f;
	ClearFogWeatherForecast();
}

/** 清除已经锁定的独立雾势预报；不会改变当前雾势或阶段倒计时。 */
void Board::ClearFogWeatherForecast()
{
	mFogWeatherForecastReady = false;
	mFogWeatherForecastDisrupted = false;
	mForecastFogWeatherIntensity = FogWeatherIntensity::DEFAULT;
	mActualForecastFogWeatherIntensity = FogWeatherIntensity::DEFAULT;
}

/**
 * 按“原版默认雾 → 小雾/普通迷雾/大雾事件 → 默认雾”的节奏抽取下一雾势。
 * 大雾使用动态概率，剩余概率在小雾和普通迷雾之间尽量均分。
 */
FogWeatherIntensity Board::RollNextFogWeather(int forcedRoll)
{
	if (mFogWeatherIntensity != FogWeatherIntensity::DEFAULT) {
		return FogWeatherIntensity::DEFAULT;
	}
	const int roll = forcedRoll > 0
		? std::clamp(forcedRoll, 1, 100)
		: GameRandom::Range(1, 100);
	const int denseChance = GetDenseFogChancePercent();
	if (roll <= denseChance) return FogWeatherIntensity::DENSE;
	const int smallUpperBound = denseChance + (100 - denseChance + 1) / 2;
	return roll <= smallUpperBound
		? FogWeatherIntensity::SMALL : FogWeatherIntensity::NORMAL;
}

/**
 * 锁定下一雾势预报。
 * 首版雾势预报保持准确，避免在现有仅表达雨势误报的提示端口中伪造第二种失败语义。
 */
void Board::PrepareFogWeatherForecast(int fogRoll)
{
	if (mFogWeatherForecastReady || !SupportsFogWeather()) return;
	mFogWeatherForecastDisrupted = false;
	mActualForecastFogWeatherIntensity = RollNextFogWeather(fogRoll);
	mForecastFogWeatherIntensity = mActualForecastFogWeatherIntensity;
	mFogWeatherForecastReady = true;
}

/** 进入普通迷雾或大雾事件；只改变独立雾势，不修改雨势、台风或战斗目标选择。 */
void Board::BeginFogWeather(FogWeatherIntensity intensity, float duration)
{
	if (intensity == FogWeatherIntensity::DEFAULT) {
		EndFogWeather(duration);
		return;
	}
	const bool changed = mFogWeatherIntensity != intensity;
	mFogWeatherIntensity = intensity;
	mFogWeatherTimer = std::max(duration, 0.1f);
	ClearFogWeatherForecast();
	if (changed && mPresentation && !IsWeatherPanelInterferenceActive()) {
		mPresentation->ShowCurrentWeatherNotice();
	}
}

/** 结束增强雾势并回到原版默认雾，随后重新安排下一次独立雾势抽取。 */
void Board::EndFogWeather(float defaultDuration)
{
	const bool changed = mFogWeatherIntensity != FogWeatherIntensity::DEFAULT;
	mFogWeatherIntensity = FogWeatherIntensity::DEFAULT;
	mFogWeatherTimer = std::max(defaultDuration, 0.1f);
	ClearFogWeatherForecast();
	if (changed && mPresentation && !IsWeatherPanelInterferenceActive()) {
		mPresentation->ShowCurrentWeatherNotice();
	}
}

/** 随天气导演缩短后期低压默认雾段，但始终保留可预期的短暂恢复窗口。 */
float Board::RandomDefaultFogWeatherDuration() const
{
	const float directorFactor = GetWeatherDirectorFactor();
	return GameRandom::Range(
		LerpWeatherValue(kDefaultFogWeatherDurationMin,
			kLateDefaultFogWeatherDurationMin, directorFactor),
		LerpWeatherValue(kDefaultFogWeatherDurationMax,
			kLateDefaultFogWeatherDurationMax, directorFactor));
}

/** 揭晓已经锁定的真实雾势，并给新阶段分配独立持续时间。 */
void Board::ConsumeFogWeatherForecast()
{
	if (!mFogWeatherForecastReady) PrepareFogWeatherForecast();
	const FogWeatherIntensity next = mActualForecastFogWeatherIntensity;
	if (next != FogWeatherIntensity::DEFAULT) {
		BeginFogWeather(next, GameRandom::Range(
			kElevatedFogWeatherDurationMin, kElevatedFogWeatherDurationMax));
		return;
	}
	EndFogWeather(RandomDefaultFogWeatherDuration());
}

/** 独立推进雾势阶段和预报；暂停与倍速都跟随 Board 的游戏时间。 */
void Board::UpdateFogWeather(float deltaTime)
{
	if (!SupportsFogWeather()) return;
	mFogWeatherTimer -= deltaTime;
	if (mFogWeatherTimer <= kFogWeatherForecastLeadTime
		&& !mFogWeatherForecastReady) {
		PrepareFogWeatherForecast();
	}
	if (mFogWeatherTimer <= 0.0f) {
		ConsumeFogWeatherForecast();
	}
}

/**
 * 仅超强台风累积驱散雾；较弱台风只推动雾团，并让已有驱散量继续回流。
 * 增强雾势事件被超强台风完全吹散后提前结束，并回到原版默认雾休整。
 */
void Board::UpdateFogDispersal(float deltaTime)
{
	const float dispersalRate = mTyphoonStrength == TyphoonStrength::SUPER
		? kSuperFogDispersalRate : -kFogReturnRate;
	mFogDispersal = std::clamp(
		mFogDispersal + dispersalRate * deltaTime, 0.0f, 1.0f);

	float targetOffsetX = 0.0f;
	if (HasTyphoon() && mWindDirection != WindDirection::NONE) {
		const float direction = mWindDirection == WindDirection::TOWARD_FRONT ? 1.0f : -1.0f;
		targetOffsetX = direction * kFogMaximumDriftX
			* std::max(0.2f, mFogDispersal);
	}
	const float maxOffsetDelta = kFogDriftSpeed * deltaTime;
	mFogVisualOffsetX += std::clamp(
		targetOffsetX - mFogVisualOffsetX, -maxOffsetDelta, maxOffsetDelta);

	if (mFogWeatherIntensity != FogWeatherIntensity::DEFAULT
		&& mFogDispersal >= 0.999f) {
		EndFogWeather(RandomDefaultFogWeatherDuration());
	}
}

/**
 * 把关卡基准、三档增强雾势扩展和台风驱散合成为逐格 alpha。
 * 读档收尾可直接对齐终态，常规更新则继续按填充/消散速率平滑追赶。
 */
void Board::UpdateFogCellAlpha(float deltaTime, bool snapToTarget)
{
	const int drawRows = GetFogDrawRowCount();
	const int fogCellCount = mColumns * drawRows;
	if (static_cast<int>(mFogCellAlpha.size()) != fogCellCount) {
		mFogCellAlpha.assign(fogCellCount, 0.0f);
	}
	if (fogCellCount <= 0) return;

	const int leftColumn = GetEffectiveFogLeftColumn();
	const float visibility = 1.0f - mFogDispersal;
	const float edgeAlpha = IsDenseFogWeather()
		? kDenseFogEdgeAlpha : kBaseFogEdgeAlpha;
	for (int row = 0; row < drawRows; ++row) {
		for (int col = 0; col < mColumns; ++col) {
			float target = 0.0f;
			if (col == leftColumn) target = edgeAlpha * visibility;
			else if (col > leftColumn) target = kFogInteriorAlpha * visibility;
			target *= 1.0f - GetPlanternIllumination(row, col);

			float& alpha = mFogCellAlpha[row * mColumns + col];
			if (snapToTarget) {
				alpha = target;
			}
			else {
				const float rate = target >= alpha ? kFogFillRate : kFogClearRate;
				const float maxDelta = rate * deltaTime;
				alpha += std::clamp(target - alpha, -maxDelta, maxDelta);
			}
		}
	}
}

/** 推进当前迷雾关卡的雾势、台风驱散与纹理呼吸；其他关卡保持零开销。 */
void Board::UpdateFog(float deltaTime)
{
	if (!mFogWeatherInitialized || deltaTime <= 0.0f || !SupportsStageFog()) return;
	// 暴风雨夜把大雾锁到通关；驱散、漂移与逐格 alpha 仍走原有唯一结算链。
	if (!IsStormyNightActive()) UpdateFogWeather(deltaTime);
	UpdateFogDispersal(deltaTime);
	UpdateFogCellAlpha(deltaTime, false);
	mFogAnimationTime = std::fmod(mFogAnimationTime + deltaTime, 3600.0f);
}

/** 从关卡存档恢复会影响未来雾势和台风回流的状态；逐格 alpha 在实体恢复完成后重建。 */
void Board::RestoreFogState(bool initialized, FogWeatherIntensity intensity,
	FogWeatherIntensity forecast, FogWeatherIntensity actual,
	float timer, bool forecastReady, float dispersal, float visualOffsetX)
{
	mFogWeatherInitialized = initialized && SupportsFogWeather();
	mFogWeatherIntensity = mFogWeatherInitialized
		? intensity : FogWeatherIntensity::DEFAULT;
	mFogWeatherTimer = mFogWeatherInitialized ? std::max(0.0f, timer) : 0.0f;
	mFogWeatherForecastReady = mFogWeatherInitialized && forecastReady;
	mFogWeatherForecastDisrupted = false;
	mForecastFogWeatherIntensity = mFogWeatherForecastReady
		? forecast : FogWeatherIntensity::DEFAULT;
	mActualForecastFogWeatherIntensity = mFogWeatherForecastReady
		? actual : FogWeatherIntensity::DEFAULT;
	mFogDispersal = mFogWeatherInitialized
		? std::clamp(dispersal, 0.0f, 1.0f) : 0.0f;
	mFogVisualOffsetX = mFogWeatherInitialized
		? std::clamp(visualOffsetX, -kFogMaximumDriftX, kFogMaximumDriftX) : 0.0f;
	mFogAnimationTime = 0.0f;
	mFogCellAlpha.assign(SupportsStageFog() ? mColumns * (mRows + 1) : 0, 0.0f);
}

void Board::RefreshZombieWeatherSpeeds()
{
	for (int id : mEntityRegistry.GetAllZombieIDs()) {
		Zombie* zombie = mEntityRegistry.GetZombie(id);
		if (zombie) zombie->RefreshAnimSpeedForWeather();
	}
}

void Board::EmitRainEffect(float duration)
{
	if (!g_particleSystem || mRainIntensity == RainIntensity::CLEAR || duration <= 0.0f) return;
	const std::string effectName = PrecipitationEffectName(
		mRainIntensity, mWindDirection, IsWinterPrecipitationSnow());
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

/** 在当前地形的逻辑网格内随机选择落点，播放短促的原版雨滴水花与扩散圆圈。 */
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

	const float splashX = GameRandom::Range(minX, maxX);
	float splashY = GameRandom::Range(minY, maxY);
	if (IsRoofBackground()) {
		// 屋顶每行是随 X 偏移的斜带；先选行，再在该行内部取局部 Y，避免矩形采样落到天空。
		const int row = GameRandom::Range(0, mRows - 1);
		const float halfHeight = mCellHeight * 0.5f;
		splashY = GetRowCenterYAtX(row, splashX) + GameRandom::Range(
			-halfHeight + kRainSplashEdgePadding,
			halfHeight - kRainSplashEdgePadding);
	}
	g_particleSystem->EmitEffect("RainGroundSplash", Vector(splashX, splashY),
		LAYER_EFFECTS_WORLD);
}

/** 推进地面水花节奏；计时器是纯视觉状态，雨势切换和读档后都会重新起拍。 */
void Board::UpdateRainGroundSplash(float deltaTime)
{
	if (mRainIntensity == RainIntensity::CLEAR || IsWinterPrecipitationSnow()) return;
	mRainSplashTimer -= deltaTime;
	if (mRainSplashTimer > 0.0f) return;

	TriggerRainGroundSplash();
	mRainSplashTimer = RandomRainSplashDelay(mRainIntensity);
}

bool Board::IsRainEffectEmitting() const
{
	return g_particleSystem && mRainIntensity != RainIntensity::CLEAR
		&& g_particleSystem->IsEffectEmitting(
			PrecipitationEffectName(mRainIntensity, mWindDirection,
				IsWinterPrecipitationSnow()));
}

void Board::StartRainAudio()
{
	if (SupportsWinterTemperature()
		&& mAmbientTemperatureC <= kWinterFreezeTemperatureC) {
		StopRainAudio();
		return;
	}
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
	mWeatherForecastDisrupted = false;
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

/** 判断本次预警是否需要锁定大雨等级：公开报大雨用于真假一致的警报，真实新大雨用于切档。 */
bool Board::NeedsPendingHeavyForecastState() const
{
	return mWeatherForecastReady
		&& (mForecastRainIntensity == RainIntensity::HEAVY
			|| (mActualForecastRainIntensity == RainIntensity::HEAVY
				&& mRainIntensity != RainIntensity::HEAVY));
}

/**
 * 公开预报为大雨时锁定同分布的警报等级；真实下一段为新大雨时同一状态也是待生效台风初态。
 * 这里只消费随机数，不提前改变当前天气、台风保底或玩法倍率；只有真实新大雨切档才兑现结果。
 */
void Board::PreparePendingHeavyTyphoon(int chanceRoll, int strengthRoll)
{
	if (mPendingHeavyTyphoonPrepared || !NeedsPendingHeavyForecastState()) return;

	mPendingHeavyTyphoonPrepared = true;
	mPendingHeavyTyphoonStrength = TyphoonStrength::NONE;
	mPendingHeavyWindDirection = WindDirection::NONE;
	mPendingHeavyTyphoonStrengthTimer = 0.0f;
	mPendingHeavyWindDirectionTimer = 0.0f;
	mPendingHeavyWindGustTimer = 0.0f;
	mPendingHeavyTyphoonGustsRemaining = 0;
	mPendingHeavyRainPromptVariant = GameRandom::Range(0, 2);
	if (!SupportsTyphoon()) return;
	mPendingHeavyTyphoonOpeningProtected = IsOpeningTyphoonProtectionActive();
	if (mPendingHeavyTyphoonOpeningProtected) return;

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
	mPendingHeavyTyphoonOpeningProtected = false;
	mPendingHeavyTyphoonStrength = TyphoonStrength::NONE;
	mPendingHeavyWindDirection = WindDirection::NONE;
	mPendingHeavyTyphoonStrengthTimer = 0.0f;
	mPendingHeavyWindDirectionTimer = 0.0f;
	mPendingHeavyWindGustTimer = 0.0f;
	mPendingHeavyTyphoonGustsRemaining = 0;
	mPendingHeavyRainPromptVariant = 0;
	mHeavyRainPromptShown = false;
}

/** 公开预报为大雨时，在揭晓前最后 5 个游戏秒显示一次已锁定等级的风暴警报。 */
void Board::MaybeShowHeavyRainPrompt()
{
	if (mHeavyRainPromptShown || !mWeatherForecastReady
		|| mWeatherForecastDisrupted
		|| IsWeatherPanelInterferenceActive()
		|| mForecastRainIntensity != RainIntensity::HEAVY
		|| mWeatherTimer > kHeavyRainPromptLeadTime || !mPresentation) return;
	if (!mPendingHeavyTyphoonPrepared) PreparePendingHeavyTyphoon();
	if (!mPendingHeavyTyphoonPrepared) return;
	mPresentation->ShowHeavyRainWarning(
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

/** 揭晓锁定的真实天气；雨势或台风等级错误时通知场景显示非模态失败提示。 */
void Board::ConsumeWeatherForecast()
{
	if (!mWeatherForecastReady) PrepareWeatherForecast();
	const RainIntensity forecast = mForecastRainIntensity;
	const RainIntensity next = mActualForecastRainIntensity;
	// 大雨续期不会兑现新抽取的 pending 台风，实际等级仍是揭晓时正在生效的台风。
	// 新大雨则会消费同一份 pending 初态，因此预报和实际等级在正常路径中保持一致。
	const TyphoonStrength forecastTyphoon = forecast == RainIntensity::HEAVY
		&& mPendingHeavyTyphoonPrepared
		? mPendingHeavyTyphoonStrength : TyphoonStrength::NONE;
	const TyphoonStrength actualTyphoon = next == RainIntensity::HEAVY
		? (mRainIntensity == RainIntensity::HEAVY
			? mTyphoonStrength
			: (mPendingHeavyTyphoonPrepared
				? mPendingHeavyTyphoonStrength : TyphoonStrength::NONE))
		: TyphoonStrength::NONE;
	if (!mWeatherForecastDisrupted && !IsWeatherPanelInterferenceActive()
		&& (forecast != next || forecastTyphoon != actualTyphoon) && mPresentation) {
		mPresentation->ShowWeatherForecastFailure(
			forecast, next, forecastTyphoon, actualTyphoon);
	}
	mWeatherForecastReady = false;
	mWeatherForecastDisrupted = false;
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
	if (!SupportsTyphoon()) return;
	// 保护期不是一次随机落空，不能累计台风 pity，否则第 6 波会被反向推成近似必出。
	if (IsOpeningTyphoonProtectionActive()) return;
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
	if (!SupportsTyphoon()) {
		ClearPendingHeavyRainWarning();
		StopTyphoon();
		return;
	}
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
	const bool openingProtected = mPendingHeavyTyphoonOpeningProtected;
	ClearPendingHeavyRainWarning();

	if (strength == TyphoonStrength::NONE) {
		StopTyphoon();
		if (!openingProtected) {
			mHeavyPhasesWithoutTyphoon = std::min(
				mHeavyPhasesWithoutTyphoon + 1, kTyphoonPityMaxMisses);
		}
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

/** 从存档恢复大雨预报警报等级或尚未生效的真实台风结果；损坏组合只清空，不重 roll。 */
void Board::RestorePendingHeavyTyphoon(bool prepared, bool openingProtected,
	TyphoonStrength strength,
	WindDirection direction, float strengthTimer, float gustTimer,
	float directionTimer, int gustsRemaining, int promptVariant)
{
	ClearPendingHeavyRainWarning();
	if (!prepared || !NeedsPendingHeavyForecastState()) return;
	if (!SupportsTyphoon()) {
		mPendingHeavyTyphoonPrepared = true;
		mPendingHeavyRainPromptVariant = std::clamp(promptVariant, 0, 2);
		return;
	}
	const bool validStrength = strength >= TyphoonStrength::NONE
		&& strength <= TyphoonStrength::SUPER;
	const bool validDirection = strength == TyphoonStrength::NONE
		? direction == WindDirection::NONE
		: (direction == WindDirection::TOWARD_HOUSE
			|| direction == WindDirection::TOWARD_FRONT);
	if (!validStrength || !validDirection
		|| (openingProtected && strength != TyphoonStrength::NONE)) return;

	mPendingHeavyTyphoonPrepared = true;
	mPendingHeavyTyphoonOpeningProtected = openingProtected;
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

/** 夹紧并恢复当前波已经正式生成的鎏金冰车数量。 */
void Board::RestoreGildedZamboniWaveSpawnCount(int count)
{
	mGildedZambonisSpawnedThisWave = std::clamp(count, 0, kGildedZamboniMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的精英海豚骑士数量。 */
void Board::RestoreEliteDolphinRiderWaveSpawnCount(int count)
{
	mEliteDolphinRidersSpawnedThisWave =
		std::clamp(count, 0, kEliteDolphinRiderMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的精英小丑数量。 */
void Board::RestoreEliteJackInTheBoxWaveSpawnCount(int count)
{
	mEliteJackInTheBoxesSpawnedThisWave =
		std::clamp(count, 0, kEliteJackInTheBoxMaxPerWave);
}

void Board::RestoreEliteDiggerWaveSpawnCount(int count)
{
	mEliteDiggersSpawnedThisWave = std::clamp(count, 0, kEliteDiggerMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的精英跳跳数量。 */
void Board::RestoreElitePogoWaveSpawnCount(int count)
{
	mElitePogosSpawnedThisWave = std::clamp(count, 0, kElitePogoMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的精英扶梯数量。 */
void Board::RestoreEliteLadderWaveSpawnCount(int count)
{
	mEliteLaddersSpawnedThisWave = std::clamp(count, 0, kEliteLadderMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的导流投篮车数量。 */
void Board::RestoreEliteCatapultWaveSpawnCount(int count)
{
	mEliteCatapultsSpawnedThisWave = std::clamp(
		count, 0, kEliteCatapultMaxPerWave);
}

/** 恢复冒险红眼的波次名额；无尽不消费也不保留该上限计数。 */
void Board::RestoreRedeyeGargantuarWaveSpawnCount(int count)
{
	mRedeyeGargantuarsSpawnedThisWave = mIsSurvival ? 0 : std::clamp(
		count, 0, kRedeyeGargantuarMaxPerAdventureWave);
}

/** 夹紧并恢复当前波已经正式生成的绝缘僵尸数量。 */
void Board::RestoreInsulatorWaveSpawnCount(int count)
{
	mInsulatorsSpawnedThisWave = std::clamp(count, 0, kInsulatorMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的劫持者数量。 */
void Board::RestoreHijackerWaveSpawnCount(int count)
{
	mHijackersSpawnedThisWave = std::clamp(count, 0, kHijackerMaxPerWave);
}

/** 恢复成功处决后的波次冷却；旧档缺字段时由调用方传入中性零值。 */
void Board::RestoreHijackerSpawnCooldown(int wavesRemaining, bool blockedThisWave)
{
	mHijackerSpawnCooldownWavesRemaining = std::clamp(
		wavesRemaining, 0, kHijackerSpawnCooldownWaves);
	mHijackerSpawnBlockedThisWave = blockedThisWave;
}

/** 成功释放立即封锁本波余下候选，并从下一波起完整跳过两波。 */
void Board::BeginHijackerSpawnCooldown()
{
	mHijackerSpawnCooldownWavesRemaining = kHijackerSpawnCooldownWaves;
	mHijackerSpawnBlockedThisWave = true;
}

/** 在正式生成前消费一份未来冷却，封锁标志保留到下一次换波。 */
void Board::AdvanceHijackerSpawnCooldownForNewWave()
{
	mHijackerSpawnBlockedThisWave = mHijackerSpawnCooldownWavesRemaining > 0;
	if (mHijackerSpawnCooldownWavesRemaining > 0) {
		--mHijackerSpawnCooldownWavesRemaining;
	}
}

/** 夹紧并恢复当前波已经正式生成的接地僵尸数量。 */
void Board::RestoreGroundingZombieWaveSpawnCount(int count)
{
	mGroundingZombiesSpawnedThisWave = std::clamp(
		count, 0, kGroundingZombieMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的雪橇车队数量；三名跟随者不计入此值。 */
void Board::RestoreBobsledTeamWaveSpawnCount(int count)
{
	mBobsledTeamsSpawnedThisWave = std::clamp(count, 0, kBobsledTeamMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的冰墙工程师数量。 */
void Board::RestoreIceWallEngineerWaveSpawnCount(int count)
{
	mIceWallEngineersSpawnedThisWave = std::clamp(
		count, 0, kIceWallEngineerMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的冰裂钻机数量。 */
void Board::RestoreIceCrackDrillWaveSpawnCount(int count)
{
	mIceCrackDrillsSpawnedThisWave = std::clamp(
		count, 0, kIceCrackDrillMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的气象干扰僵尸数量。 */
void Board::RestoreWeatherJammerWaveSpawnCount(int count)
{
	mWeatherJammersSpawnedThisWave = std::clamp(
		count, 0, kWeatherJammerMaxPerWave);
}

/** 夹紧并恢复当前波已经正式生成的冰像处刑者数量。 */
void Board::RestoreIceStatueExecutionerWaveSpawnCount(int count)
{
	mIceStatueExecutionersSpawnedThisWave = std::clamp(
		count, 0, kIceStatueExecutionerMaxPerWave);
}

/** 旧档缺字段时以零值恢复；本波计数按当前关卡的实际限额夹紧。 */
void Board::RestoreSnowBurrowSpawnState(int count, bool tutorialHoleSpawnConsumed)
{
	const int maxPerWave = mLevel == kSnowBurrowTutorialLevel
		? kSnowBurrowTutorialMaxPerWave : kSnowBurrowCompositeMaxPerWave;
	mSnowBurrowsSpawnedThisWave = std::clamp(count, 0, maxPerWave);
	mSnowBurrowTutorialHoleSpawnConsumed = tutorialHoleSpawnConsumed;
}

void Board::RestoreAdaptiveHelmetSpawnState(
	int waveCount, bool tutorialWaveSpawned)
{
	const bool tutorialLevel = !mIsSurvival
		&& mLevel == kAdaptiveHelmetTutorialLevel;
	mAdaptiveHelmetsSpawnedThisWave = std::clamp(
		waveCount, 0, kAdaptiveHelmetMaxPerWave);
	mAdaptiveHelmetTutorialWaveSpawned = tutorialLevel
		&& tutorialWaveSpawned;
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
	mLastTyphoonBlockedPlantSteps = 0;
	RefreshZombieWeatherSpeeds();
}

/** 从存档恢复已经判定过的台风结果；无效组合只会安全退化为无台风，不重新随机。 */
void Board::RestoreTyphoonState(TyphoonStrength strength, WindDirection direction,
	float strengthTimer, float gustTimer, float directionTimer, int gustsRemaining)
{
	StopTyphoon();
	if (!SupportsTyphoon()) return;
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
		if (mPresentation && !IsWeatherPanelInterferenceActive()) {
			mPresentation->ShowCurrentWeatherNotice();
		}
		return;
	}
	mTyphoonStrength = next;
	mTyphoonStrengthTimer = RandomTyphoonStrengthDuration(next);
	mTyphoonGustsRemaining = std::min(mTyphoonGustsRemaining, TyphoonMaxGusts(next));
	mWindGustTimer = mTyphoonGustsRemaining > 0
		? RandomTyphoonGustInterval(next) : 0.0f;
	mWindParticleTimer = 0.0f;
	RefreshZombieWeatherSpeeds();
	if (mPresentation && !IsWeatherPanelInterferenceActive()) {
		mPresentation->ShowCurrentWeatherNotice();
	}
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
	if (selected == ZombieType::ZOMBIE_GILDED_ZAMBONI) {
		if (mGildedZambonisSpawnedThisWave >= kGildedZamboniMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mGildedZambonisSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ELITE_DOLPHIN_RIDER) {
		if (mEliteDolphinRidersSpawnedThisWave >= kEliteDolphinRiderMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mEliteDolphinRidersSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ELITE_JACK_IN_THE_BOX) {
		if (mEliteJackInTheBoxesSpawnedThisWave >= kEliteJackInTheBoxMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mEliteJackInTheBoxesSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ELITE_DIGGER) {
		if (mEliteDiggersSpawnedThisWave >= kEliteDiggerMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mEliteDiggersSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ELITE_POGO) {
		if (mElitePogosSpawnedThisWave >= kElitePogoMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mElitePogosSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ELITE_LADDER) {
		if (mEliteLaddersSpawnedThisWave >= kEliteLadderMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mEliteLaddersSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ELITE_CATAPULT) {
		if (mEliteCatapultsSpawnedThisWave >= kEliteCatapultMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mEliteCatapultsSpawnedThisWave;
	}
	// 红眼只在冒险波次限额；无尽刻意不读写此计数，保留其原有的无限压力曲线。
	if (!mIsSurvival && selected == ZombieType::ZOMBIE_REDEYE_GARGANTUAR) {
		if (mRedeyeGargantuarsSpawnedThisWave >= kRedeyeGargantuarMaxPerAdventureWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mRedeyeGargantuarsSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_INSULATOR) {
		if (mInsulatorsSpawnedThisWave >= kInsulatorMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mInsulatorsSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_HIJACKER) {
		const bool forcedTutorialWave = !mIsSurvival
			&& mLevel == kHijackerTutorialLevel
			&& mCurrentWave == kHijackerTutorialWave;
		if ((!forcedTutorialWave && IsHijackerSpawnBlocked())
			|| mHijackersSpawnedThisWave >= kHijackerMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mHijackersSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_GROUNDING) {
		if (mGroundingZombiesSpawnedThisWave >= kGroundingZombieMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mGroundingZombiesSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_BOBSLED_TEAM) {
		if (mBobsledTeamsSpawnedThisWave >= kBobsledTeamMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mBobsledTeamsSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ICE_WALL_ENGINEER) {
		if (mIceWallEngineersSpawnedThisWave >= kIceWallEngineerMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mIceWallEngineersSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ICE_CRACK_DRILL) {
		if (mIceCrackDrillsSpawnedThisWave >= kIceCrackDrillMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mIceCrackDrillsSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_WEATHER_JAMMER) {
		if (mWeatherJammersSpawnedThisWave >= kWeatherJammerMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mWeatherJammersSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ICE_STATUE_EXECUTIONER) {
		if (mIceStatueExecutionersSpawnedThisWave
			>= kIceStatueExecutionerMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mIceStatueExecutionersSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_SNOW_BURROW) {
		const int maxPerWave = mLevel == kSnowBurrowTutorialLevel
			? kSnowBurrowTutorialMaxPerWave : kSnowBurrowCompositeMaxPerWave;
		if (mSnowBurrowsSpawnedThisWave >= maxPerWave
			|| CountActiveOrPendingZombieType(selected) >= maxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mSnowBurrowsSpawnedThisWave;
	}
	if (selected == ZombieType::ZOMBIE_ADAPTIVE_HELMET) {
		const bool tutorialLevel = !mIsSurvival
			&& mLevel == kAdaptiveHelmetTutorialLevel;
		if ((tutorialLevel && !mPolarFirstWhiteoutCompleted)
			|| mAdaptiveHelmetsSpawnedThisWave >= kAdaptiveHelmetMaxPerWave) {
			return ZombieType::NUM_ZOMBIE_TYPES;
		}
		++mAdaptiveHelmetsSpawnedThisWave;
	}
	return ResolveRainMutationType(selected, mutationRoll);
}

int Board::CountActiveOrPendingZombieType(ZombieType type) const
{
	int count = static_cast<int>(std::count_if(mPendingSnowHoleSpawns.begin(),
		mPendingSnowHoleSpawns.end(), [type](const PendingSnowHoleSpawn& pending) {
			return pending.type == type;
		}));
	for (int id : mEntityRegistry.GetAllZombieIDs()) {
		Zombie* zombie = mEntityRegistry.GetZombie(id);
		if (zombie && zombie->IsActive() && !zombie->IsDying()
			&& !zombie->IsMindControlled() && zombie->mZombieType == type) {
			++count;
		}
	}
	return count;
}

Zombie* Board::CreateResolvedWaveZombie(ZombieType actualType, int row, float x)
{
	const ZombieType terrainType = ResolveTerrainZombieType(actualType, row);
	Zombie* zombie = CreateZombie(terrainType, row, x);
	// 只在正式候选真正创建成功后解锁；解析失败、预览、读档和通用直造都不能伪造遭遇。
	if (zombie && actualType == ZombieType::ZOMBIE_ELITE_DANCER) {
		GameAPP::GetInstance().RecordEliteDancerEncounter();
	}
	return zombie;
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

bool Board::RedirectTyphoonFromBlover(WindDirection direction)
{
	if (!HasTyphoon()
		|| (direction != WindDirection::TOWARD_HOUSE
			&& direction != WindDirection::TOWARD_FRONT)) {
		return false;
	}

	const bool changed = mWindDirection != direction;
	mWindDirection = direction;
	// 玩家主动改向后重新获得一个完整方向阶段，避免旧计时恰好归零而在下一帧立刻随机翻回。
	mWindDirectionTimer = GameRandom::Range(
		kWindDirectionDurationMin, kWindDirectionDurationMax);
	if (mTyphoonGustActive) {
		// 活动阵风的锁定方向也是玩法权威：后续僵尸漂移与尚未结算的植物换格立即跟随。
		mActiveGustDirection = direction;
	}
	if (changed) {
		mWindParticleTimer = 0.0f;
		RestartRainVisualForWindChange();
	}
	return true;
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
	if (!SupportsTyphoon()) {
		if (HasTyphoon()) StopTyphoon();
		return;
	}
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
	if (IsStormyNightActive()) {
		// 4-9 终局固定强台风；只保留风向重抽和一次性阵风预算，不走强度衰减。
		mTyphoonStrengthTimer = kStormyNightLockedDuration;
	}
	else {
		mTyphoonStrengthTimer -= deltaTime;
		if (mTyphoonStrengthTimer <= 0.0f) WeakenTyphoon();
	}
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
	if (!SupportsTyphoon() || !HasTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| mWindDirection == WindDirection::NONE || mTyphoonGustActive) return false;
	mLastTyphoonMovedPlants = 0;
	mLastTyphoonLostPlants = 0;
	mLastTyphoonBlockedPlantSteps = 0;
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
 * 锚定植物所在格自身不移动，只对直接撞入该格的植物逐格派发撞击，不传导后方植物链；
 * 植物被吹出棋盘或吹入弹坑时死亡，弹坑不能反直觉地充当挡风墙。
 * 每次换格先更新 Cell、row/column 与碰撞箱，再让植物画面用瞬态偏移追赶；
 * 因此滑动中保存只会记录目标格，读档不会恢复半格状态或重复位移。
 */
void Board::TriggerTyphoonPlantMove(TyphoonStrength strength, WindDirection direction)
{
	mLastTyphoonMovedPlants = 0;
	mLastTyphoonLostPlants = 0;
	mLastTyphoonBlockedPlantSteps = 0;
	if (!HasTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| direction == WindDirection::NONE) return;

	const int columnDelta = direction == WindDirection::TOWARD_FRONT ? 1 : -1;
	const int distance = TyphoonGustDistance(strength);
	std::unordered_set<int> movedPlantIDs;
	std::unordered_set<int> lostPlantIDs;
	std::unordered_set<int> anchorFeedbackPlantIDs;
	for (int step = 0; step < distance; ++step) {
		for (int row = 0; row < mRows; ++row) {
			const int firstColumn = columnDelta > 0 ? mColumns - 1 : 0;
			const int endColumn = columnDelta > 0 ? -1 : mColumns;
			for (int column = firstColumn; column != endColumn; column -= columnDelta) {
				Cell* source = GetCell(row, column);
				if (!source || source->IsEmpty()) continue;
				Ladder* sourceLadder = GetLadderAt(row, column);
				const int underID = source->GetUnderPlantID();
				const int normalID = source->GetNormalPlantID();
				const int pumpkinID = source->GetPumpkinPlantID();
				const int overlayID = source->GetOverlayPlantID();
				Plant* under = mEntityRegistry.GetPlant(underID);
				Plant* normal = mEntityRegistry.GetPlant(normalID);
				Plant* pumpkin = mEntityRegistry.GetPlant(pumpkinID);
				Plant* overlay = mEntityRegistry.GetPlant(overlayID);
				if (!under || !under->IsActive()) {
					source->ClearUnderPlantID();
					under = nullptr;
				}
				if (!normal || !normal->IsActive()) {
					source->ClearNormalPlantID();
					normal = nullptr;
				}
				if (!pumpkin || !pumpkin->IsActive()) {
					source->ClearPumpkinPlantID();
					pumpkin = nullptr;
				}
				if (!overlay || !overlay->IsActive()) {
					source->ClearOverlayPlantID();
					overlay = nullptr;
				}
				if (!under && !normal && !pumpkin && !overlay) continue;
				// 冰像封存期间整个植物组合都与外部玩法隔离；台风不得先改 Cell 再让
				// Plant::MoveToGridCell 拒绝，从而制造逻辑格与实体坐标分裂。
				if ((under && under->IsIceSealed())
					|| (normal && normal->IsIceSealed())
					|| (pumpkin && pumpkin->IsIceSealed())
					|| (overlay && overlay->IsIceSealed())) {
					continue;
				}

				if (normal && IsMultiCellPlantType(normal->mPlantType)) {
					// 次级占格只是一株实体的别名；整株只在逻辑锚点处原子结算一次。
					if (normal->mRow != row || normal->mColumn != column) continue;
					struct FootprintStack {
						Cell* cell = nullptr;
						int row = -1;
						int column = -1;
						std::array<int, 4> ids{};
						std::array<Plant*, 4> plants{};
						Ladder* ladder = nullptr;
					};
					const PlantFootprint footprint = GetPlantFootprint(normal->mPlantType);
					std::vector<FootprintStack> stacks;
					stacks.reserve(footprint.count);
					std::unordered_set<int> sourcePlantIDs;
					bool anchored = false;
					for (std::size_t i = 0; i < footprint.count; ++i) {
						FootprintStack stack;
						stack.row = row + footprint.cells[i].rowOffset;
						stack.column = column + footprint.cells[i].columnOffset;
						stack.cell = GetCell(stack.row, stack.column);
						if (!stack.cell) continue;
						stack.ids = { stack.cell->GetUnderPlantID(),
							stack.cell->GetNormalPlantID(), stack.cell->GetPumpkinPlantID(),
							stack.cell->GetOverlayPlantID() };
						for (std::size_t layer = 0; layer < stack.ids.size(); ++layer) {
							Plant* member = mEntityRegistry.GetPlant(stack.ids[layer]);
							if (!member || !member->IsActive()) {
								stack.ids[layer] = NULL_PLANT_ID;
								continue;
							}
							stack.plants[layer] = member;
							sourcePlantIDs.insert(member->mPlantID);
							anchored = anchored || member->AnchorsPlantCellAgainstTyphoon();
						}
						stack.ladder = GetLadderAt(stack.row, stack.column);
						stacks.push_back(stack);
					}
					if (stacks.size() != footprint.count || anchored) continue;

					bool lost = false;
					bool blocked = false;
					Plant* blockingAnchor = nullptr;
					for (const FootprintStack& stack : stacks) {
						const int targetColumn = stack.column + columnDelta;
						if (targetColumn < 0 || targetColumn >= mColumns
							|| HasCraterAt(stack.row, targetColumn)) {
							lost = true;
							break;
						}
						Cell* target = GetCell(stack.row, targetColumn);
						if (!target) {
							blocked = true;
							break;
						}
						for (const int targetID : { target->GetUnderPlantID(),
							target->GetNormalPlantID(), target->GetPumpkinPlantID(),
							target->GetOverlayPlantID() }) {
							if (targetID != NULL_PLANT_ID
								&& sourcePlantIDs.find(targetID) == sourcePlantIDs.end()) {
								blocked = true;
								blockingAnchor = GetTopPlantAt(stack.row, targetColumn);
								break;
							}
						}
						if (blocked) break;
					}
					if (lost) {
						for (const int id : sourcePlantIDs) {
							if (Plant* member = mEntityRegistry.GetPlant(id)) {
								lostPlantIDs.insert(id);
								member->Die();
							}
						}
						continue;
					}
					if (blocked) {
						if (blockingAnchor && blockingAnchor->AnchorsPlantCellAgainstTyphoon()) {
							const bool showFeedback = anchorFeedbackPlantIDs
								.insert(blockingAnchor->mPlantID).second;
							blockingAnchor->OnTyphoonPlantImpact(showFeedback);
							++mLastTyphoonBlockedPlantSteps;
						}
						continue;
					}

					// 先清空全部源格，再按相对格映射写入目标，避免相邻格重叠时覆盖尚未搬走的承载物。
					for (const FootprintStack& stack : stacks) {
						if (stack.cell->GetUnderPlantID() == stack.ids[0]) stack.cell->ClearUnderPlantID();
						if (stack.cell->GetNormalPlantID() == stack.ids[1]) stack.cell->ClearNormalPlantID();
						if (stack.cell->GetPumpkinPlantID() == stack.ids[2]) stack.cell->ClearPumpkinPlantID();
						if (stack.cell->GetOverlayPlantID() == stack.ids[3]) stack.cell->ClearOverlayPlantID();
					}
					std::unordered_set<int> repositionedPlantIDs;
					for (const FootprintStack& stack : stacks) {
						const int targetColumn = stack.column + columnDelta;
						Cell* target = GetCell(stack.row, targetColumn);
						if (stack.ids[0] != NULL_PLANT_ID) target->SetUnderPlantID(stack.ids[0]);
						if (stack.ids[1] != NULL_PLANT_ID) target->SetNormalPlantID(stack.ids[1]);
						if (stack.ids[2] != NULL_PLANT_ID) target->SetPumpkinPlantID(stack.ids[2]);
						if (stack.ids[3] != NULL_PLANT_ID) target->SetOverlayPlantID(stack.ids[3]);
						for (Plant* member : stack.plants) {
							if (member && repositionedPlantIDs.insert(member->mPlantID).second) {
								const int memberTargetColumn = member == normal
									? normal->mColumn + columnDelta : targetColumn;
								member->MoveToGridCell(stack.row, memberTargetColumn,
									kTyphoonPlantSlideDuration);
								movedPlantIDs.insert(member->mPlantID);
							}
						}
						if (stack.ladder) stack.ladder->MoveToGridCell(stack.row, targetColumn);
						RefreshPlantStackRenderOrder(target);
					}
					continue;
				}
				const Plant* sourceAnchor = pumpkin && pumpkin->AnchorsPlantCellAgainstTyphoon()
					? pumpkin
					: (normal && normal->AnchorsPlantCellAgainstTyphoon()
						? normal
						: (under && under->AnchorsPlantCellAgainstTyphoon() ? under : nullptr));
				if (sourceAnchor) continue;

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
					if (pumpkin) {
						lostPlantIDs.insert(pumpkinID);
						pumpkin->Die();
					}
					if (overlay) {
						lostPlantIDs.insert(overlayID);
						overlay->Die();
					}
					continue;
				}

				Cell* target = GetCell(row, targetColumn);
				if (!target) continue;
				if (!target->IsEmpty()) {
					Plant* anchor = GetTopPlantAt(row, targetColumn);
					if (anchor && anchor->AnchorsPlantCellAgainstTyphoon()) {
						const bool showFeedback =
							anchorFeedbackPlantIDs.insert(anchor->mPlantID).second;
						anchor->OnTyphoonPlantImpact(showFeedback);
						++mLastTyphoonBlockedPlantSteps;
					}
					continue;
				}
				if (HasCraterAt(row, targetColumn)) {
					if (under) {
						lostPlantIDs.insert(underID);
						under->Die();
					}
					if (normal) {
						lostPlantIDs.insert(normalID);
						normal->Die();
					}
					if (pumpkin) {
						lostPlantIDs.insert(pumpkinID);
						pumpkin->Die();
					}
					if (overlay) {
						lostPlantIDs.insert(overlayID);
						overlay->Die();
					}
					continue;
				}
				source->ClearUnderPlantID();
				source->ClearNormalPlantID();
				source->ClearPumpkinPlantID();
				source->ClearOverlayPlantID();
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
				if (pumpkin) {
					target->SetPumpkinPlantID(pumpkinID);
					pumpkin->MoveToGridCell(row, targetColumn, kTyphoonPlantSlideDuration);
					movedPlantIDs.insert(pumpkinID);
				}
				if (overlay) {
					target->SetOverlayPlantID(overlayID);
					overlay->MoveToGridCell(row, targetColumn, kTyphoonPlantSlideDuration);
					movedPlantIDs.insert(overlayID);
				}
				if (sourceLadder) {
					// 扶梯是植物组合的格附件：逻辑格与 Cell 同帧切换，绘制继续共享宿主的追赶偏移。
					if (Ladder* staleTarget = GetLadderAt(row, targetColumn);
						staleTarget && staleTarget != sourceLadder) {
						RemoveLadderAt(row, targetColumn);
					}
					sourceLadder->MoveToGridCell(row, targetColumn);
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
	mWeatherForecastDisrupted = false;
	mRainSplashTimer = RandomRainSplashDelay(intensity);
	mLightningTimer = (intensity == RainIntensity::HEAVY
		&& !IsWinterPrecipitationSnow())
		? GameRandom::Range(kLightningDelayMin, kLightningDelayMax)
		: 0.0f;
	mRainVisualActive = false;
	RefreshZombieWeatherSpeeds();
	// 同档续期也新建发射器，让旧雨丝自然收尾并与新雨段无缝衔接。
	EmitRainEffect(duration);
	StartRainAudio();
	if (mPresentation && !IsWeatherPanelInterferenceActive()) {
		mPresentation->ShowCurrentWeatherNotice();
	}
}

/**
 * 督军天气命令只沿雨势强度向上覆盖，并复用 Board 的正式切档、粒子、声音和存档字段。
 * 周期中雨不续同档，避免靠固定指挥节奏把雨永久锁住；残血大雨可一次性补足最低持续时间。
 */
bool Board::TriggerRoofMarshalWeather(RainIntensity target, float duration,
	bool extendSameIntensity)
{
	if (!SupportsWeather() || !mWeatherInitialized || IsStormyNightActive()
		|| duration <= 0.0f
		|| (target != RainIntensity::MEDIUM && target != RainIntensity::HEAVY)) {
		return false;
	}

	const int currentRank = static_cast<int>(mRainIntensity);
	const int targetRank = static_cast<int>(target);
	if (currentRank > targetRank) return false;
	if (mRainIntensity == target
		&& (!extendSameIntensity || mWeatherTimer >= duration)) {
		return false;
	}

	const float appliedDuration = mRainIntensity == target
		? std::max(mWeatherTimer, duration)
		: duration;
	// 督军只号令雨势，不额外抽取台风；自然大雨已存在时同档延长会保留其既有台风状态。
	BeginRain(target, appliedDuration, false, false, false);
	return true;
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
	mWeatherForecastDisrupted = false;
	mRainVisualActive = false;
	mRainVisualEffectName.clear();
	RefreshZombieWeatherSpeeds();
	if (mWeatherTransitionTimer > 0.0f) StartRainAudio();
	else StopRainAudio();
	if (mPresentation && !IsWeatherPanelInterferenceActive()) {
		mPresentation->ShowCurrentWeatherNotice();
	}
}

void Board::TriggerLightning()
{
	if (mRainIntensity != RainIntensity::HEAVY || IsWinterPrecipitationSnow()) return;
	// 黑夜屋顶把现有大雨闪电作为独立雷荷的一次增量；不改变雨势或坡面径流。
	AddNightRoofCharge(kNightRoofChargeLightningBonus);
	if (IsStormyNightActive()) {
		// 暴风雨夜复用原版全屏短闪，不再叠加普通大雨的程序化闪电路径。
		mStormyNightFlashPattern = 3;
		mStormyNightFlashTimer = kStormFlashUnitSeconds;
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_THUNDER, kThunderSoundVolume);
		return;
	}
	if (!mPresentation) return;
	// 雷声与程序化闪电从同一触发点发起，保证自然天气与 AutoTest 路径音画同步。
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_THUNDER, kThunderSoundVolume);
	mPresentation->ShowLightningStrike(kLightningFlashDuration);
}

/**
 * 昼夜屋顶把当前雨势换算为可保留的全局积累；满值后只随机一次目标行组，
 * 依次经过可读预警和短时冲刷。晴天只缓慢排水，不会凭空触发事件。
 */
void Board::UpdateRoofRunoff(float deltaTime)
{
	if (!SupportsRoofRunoff()) {
		mRoofRunoffCharge = 0.0f;
		mRoofRunoffRetainedCharge = 0.0f;
		mRoofRunoffPhase = RoofRunoffPhase::IDLE;
		mRoofRunoffPhaseTimer = 0.0f;
		mRoofRunoffRowMask = 0;
		return;
	}
	if (deltaTime <= 0.0f) return;

	if (mRoofRunoffPhase == RoofRunoffPhase::WARNING
		|| mRoofRunoffPhase == RoofRunoffPhase::FLOWING) {
		mRoofRunoffPhaseTimer = std::max(0.0f,
			mRoofRunoffPhaseTimer - deltaTime);
		if (mRoofRunoffPhaseTimer > 0.0f) return;

		if (mRoofRunoffPhase == RoofRunoffPhase::WARNING) {
			mRoofRunoffPhase = RoofRunoffPhase::FLOWING;
			mRoofRunoffPhaseTimer = kRoofRunoffFlowDuration;
			return;
		}

		mRoofRunoffCharge = mRoofRunoffRetainedCharge;
		mRoofRunoffRetainedCharge = 0.0f;
		mRoofRunoffPhase = RoofRunoffPhase::IDLE;
		mRoofRunoffRowMask = 0;
		return;
	}

	float chargeDelta = 0.0f;
	switch (mRainIntensity) {
	case RainIntensity::CLEAR:
		chargeDelta = -kRoofRunoffClearDrainPerSecond;
		break;
	case RainIntensity::LIGHT:
		chargeDelta = kRoofRunoffLightChargePerSecond;
		break;
	case RainIntensity::MEDIUM:
		chargeDelta = kRoofRunoffMediumChargePerSecond;
		break;
	case RainIntensity::HEAVY:
		chargeDelta = kRoofRunoffHeavyChargePerSecond;
		break;
	}
	mRoofRunoffCharge = std::clamp(mRoofRunoffCharge
		+ chargeDelta * deltaTime, 0.0f, kRoofRunoffChargeMaximum);
	if (mRoofRunoffCharge < kRoofRunoffChargeMaximum || mRows <= 0) return;

	// 锁行组只抽一次并随档保存；不按敌我密度选行，避免系统暗中追打当前最拥挤防线。
	mRoofRunoffPhase = RoofRunoffPhase::WARNING;
	mRoofRunoffPhaseTimer = kRoofRunoffWarningDuration;
	// 下一轮起点和行组同时预抽并入档，避免活动阶段读档改变后续湿度。
	mRoofRunoffRetainedCharge = static_cast<float>(GameRandom::Range(
		kRoofRunoffRetainedChargeMin, kRoofRunoffRetainedChargeMax));
	const int rowCountRoll = GameRandom::Range(1,
		kRoofRunoffOneRowWeight + kRoofRunoffTwoRowWeight + kRoofRunoffThreeRowWeight);
	int rowCount = rowCountRoll <= kRoofRunoffOneRowWeight ? 1
		: (rowCountRoll <= kRoofRunoffOneRowWeight + kRoofRunoffTwoRowWeight ? 2 : 3);
	rowCount = std::min(rowCount, mRows);
	mRoofRunoffRowMask = 0;
	while (GetRoofRunoffRowCount() < rowCount) {
		mRoofRunoffRowMask |= 1 << GameRandom::Range(0, mRows - 1);
	}

	// 导流车只能替换本次已经抽中的一行，不能扩充冲刷规模；最终掩码仍是唯一入档事实。
	const int guideRow = GetRoofRunoffGuideCandidateRow();
	if (guideRow >= 0 && !IsRoofRunoffRowSelected(guideRow)) {
		const int replacedIndex = GameRandom::Range(0, rowCount - 1);
		int selectedIndex = 0;
		for (int row = 0; row < mRows; ++row) {
			if (!IsRoofRunoffRowSelected(row)) continue;
			if (selectedIndex++ != replacedIndex) continue;
			mRoofRunoffRowMask &= ~(1 << row);
			break;
		}
		mRoofRunoffRowMask |= 1 << guideRow;
	}
}

/**
 * 自然锁行只采样仍在坡段内的活动导流车；最近房屋者优先，ID 只负责同 X 稳定决胜。
 */
int Board::GetRoofRunoffGuideCandidateRow() const
{
	if (!SupportsRoofRunoff()) return -1;
	const float slopeEndX = GetRoofSlopeEndX();
	int bestRow = -1;
	int bestID = NULL_ZOMBIE_ID;
	float bestX = std::numeric_limits<float>::max();
	for (const int id : mEntityRegistry.GetAllZombieIDs()) {
		const Zombie* zombie = mEntityRegistry.GetZombie(id);
		if (!zombie || zombie->mRow < 0 || zombie->mRow >= mRows
			|| !zombie->CanGuideRoofRunoff()) {
			continue;
		}
		const float x = zombie->GetPosition().x;
		if (x > slopeEndX) continue;
		if (x < bestX || (x == bestX && (bestID == NULL_ZOMBIE_ID || id < bestID))) {
			bestX = x;
			bestID = id;
			bestRow = zombie->mRow;
		}
	}
	return bestRow;
}

/** 校验并恢复径流状态；损坏组合和非屋顶存档都回到中性状态。 */
void Board::RestoreRoofRunoffState(float charge, RoofRunoffPhase phase,
	int rowMask, float phaseTimer, float retainedCharge)
{
	mRoofRunoffCharge = 0.0f;
	mRoofRunoffRetainedCharge = 0.0f;
	mRoofRunoffPhase = RoofRunoffPhase::IDLE;
	mRoofRunoffPhaseTimer = 0.0f;
	mRoofRunoffRowMask = 0;
	if (!SupportsRoofRunoff() || !std::isfinite(charge)
		|| !std::isfinite(phaseTimer) || !std::isfinite(retainedCharge)) return;

	mRoofRunoffCharge = std::clamp(charge, 0.0f, kRoofRunoffChargeMaximum);
	if (phase != RoofRunoffPhase::WARNING && phase != RoofRunoffPhase::FLOWING) return;
	const int validRowMask = mRows > 0 ? (1 << mRows) - 1 : 0;
	rowMask &= validRowMask;
	if (rowMask == 0 || phaseTimer < 0.0f) {
		mRoofRunoffCharge = 0.0f;
		return;
	}
	mRoofRunoffCharge = kRoofRunoffChargeMaximum;
	mRoofRunoffPhase = phase;
	mRoofRunoffPhaseTimer = phaseTimer;
	mRoofRunoffRowMask = rowMask;
	mRoofRunoffRetainedCharge = std::clamp(retainedCharge,
		static_cast<float>(kRoofRunoffRetainedChargeMin),
		static_cast<float>(kRoofRunoffRetainedChargeMax));
}

/** 返回仍满足能力门禁的锁定实体；平时只按稳定 ID 查表，不扫描僵尸集合。 */
HijackerZombie* Board::GetValidNightRoofHijacker() const
{
	if (mNightRoofHijackerID == NULL_ZOMBIE_ID) return nullptr;
	auto* hijacker = dynamic_cast<HijackerZombie*>(
		mEntityRegistry.GetZombie(mNightRoofHijackerID));
	return hijacker && hijacker->CanBeNightRoofHijackerCandidate()
		? hijacker : nullptr;
}

/** 75% 边沿只遍历稀有劫持者弱索引；即使没有候选，本轮也不会在之后补选。 */
void Board::TryLockNightRoofHijacker()
{
	if (!SupportsNightRoofCharge() || mNightRoofHijackerSelectionAttempted) return;
	mNightRoofHijackerSelectionAttempted = true;
	auto hijacker = mEntityRegistry.SelectNightRoofHijacker();
	if (!hijacker) return;
	mNightRoofHijackerID = hijacker->mZombieID;
	hijacker->BeginNightRoofLock();
}

/** 满电时统一推演普通行与接地路线；有效劫持者把同一预警窗口延长到七秒。 */
void Board::BeginNightRoofChargeWarning()
{
	if (!SupportsNightRoofCharge()
		|| mNightRoofChargePhase != NightRoofChargePhase::CHARGING
		|| mRows <= 0) return;
	if (!mNightRoofHijackerSelectionAttempted) TryLockNightRoofHijacker();
	HijackerZombie* hijacker = GetValidNightRoofHijacker();
	mNightRoofCharge = kNightRoofChargeMaximum;
	mNightRoofChargePhase = NightRoofChargePhase::WARNING;
	mNightRoofHijackerWarningExtended = hijacker != nullptr;
	mNightRoofHijackerFinalizing = false;
	mNightRoofChargePhaseTimer = hijacker
		? kNightRoofHijackerWarningDuration : kNightRoofChargeWarningDuration;
	ChooseNightRoofChargeRoute(mNightRoofChargePhaseTimer);
	if (hijacker) hijacker->BeginNightRoofWarning();
}

/**
 * 在满电这一低频边沿采集纯数值快照，令所有普通行与每只现存接地僵尸共享同组 rollout。
 * 路线、行和稳定 ID 在这里一次锁定；后续死亡、换行、破甲或新出生都不会触发重选。
 */
void Board::ChooseNightRoofChargeRoute(float warningSeconds)
{
	using namespace PlantDefenseMonteCarlo;
	const auto decisionStarted = std::chrono::steady_clock::now();
	auto recordDecisionTime = [this, decisionStarted]() {
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - decisionStarted).count();
		mNightRoofChargeRouteDecisionMicros = static_cast<int>(std::clamp<int64_t>(
			elapsed, 0, std::numeric_limits<int>::max()));
	};
	mNightRoofChargeGuided = false;
	mNightRoofChargeGuideID = NULL_ZOMBIE_ID;
	mNightRoofChargeRouteUsedMonteCarlo = false;
	mNightRoofChargeRouteStats = {};
	mNightRoofChargeRouteDecisionMicros = 0;
	mNightRoofChargeRow = -1;
	if (mRows <= 0) {
		recordDecisionTime();
		return;
	}

	const std::vector<std::shared_ptr<Zombie>> guides =
		mEntityRegistry.GetNightRoofChargeGuideCandidates();
	std::vector<NightRoofChargeCandidate> candidates;
	candidates.reserve(static_cast<std::size_t>(mRows) + guides.size());
	for (int row = 0; row < mRows; ++row) {
		float damageMultiplier = 1.0f;
		for (int column = 0; column < mColumns; ++column) {
			const Plant* support = GetUnderPlantAt(row, column);
			if (support && support->IsActive() && !support->IsPreview()
				&& support->mPlantHealth > 0 && !support->IsSquished()) {
				damageMultiplier = std::max(damageMultiplier,
					support->GetNightRoofChargeZombieDamageMultiplier());
			}
		}
		NightRoofChargeCandidate candidate;
		candidate.row = row;
		candidate.resolveSeconds = warningSeconds;
		candidate.plantShutdownSeconds = kNightRoofPlantShutdownDuration;
		candidate.wetPlantShutdownSeconds = kNightRoofWetPlantShutdownDuration;
		candidate.wetSlopeColumnCount = kRoofSlopeColumnCount;
		candidate.zombieDamage = kNightRoofZombieDamage * damageMultiplier;
		candidate.wetZombieDamage = kNightRoofWetZombieDamage * damageMultiplier;
		candidate.paralysisSeconds = kNightRoofZombieParalysisDuration;
		candidate.wetParalysisSeconds = kNightRoofWetZombieParalysisDuration;
		candidate.wetSlopeEndX = GetRoofSlopeEndX();
		candidate.wetRow = IsRoofRunoffFlowing() && IsRoofRunoffRowSelected(row);
		candidates.push_back(candidate);
	}
	for (const std::shared_ptr<Zombie>& guide : guides) {
		if (!guide) continue;
		NightRoofChargeCandidate candidate;
		candidate.row = guide->mRow;
		candidate.guideZombieId = guide->mZombieID;
		candidate.resolveSeconds = warningSeconds;
		candidate.plantShutdownSeconds = kNightRoofPlantShutdownDuration;
		candidate.wetPlantShutdownSeconds = kNightRoofWetPlantShutdownDuration;
		candidate.wetSlopeColumnCount = kRoofSlopeColumnCount;
		candidate.wetRow = IsRoofRunoffFlowing()
			&& IsRoofRunoffRowSelected(guide->mRow);
		candidate.guided = true;
		candidates.push_back(candidate);
	}

	Snapshot snapshot;
	bool snapshotBuilt = false;
	if (GameAPP::GetInstance().mEnableMonteCarloAI) {
		PROFILE_SCOPE("MC.NightRoofRoute.Snapshot");
		snapshotBuilt = BuildMonteCarloCombatSnapshot(snapshot, false, true);
	}
	if (snapshotBuilt) {
		std::unordered_set<int> guideIDs;
		for (const std::shared_ptr<Zombie>& guide : guides) {
			if (guide) guideIDs.insert(guide->mZombieID);
		}
		for (ZombieSnapshot& zombie : snapshot.zombies) {
			zombie.forcedForDecision = guideIDs.find(zombie.id) != guideIDs.end();
		}

		NightRoofChargeConfig config;
		config.combat.rolloutCount = kNightRoofRouteMonteCarloRolloutCount;
		config.combat.maxZombiesPerRollout = kMonteCarloMaxZombies;
		config.combat.horizonSeconds = kNightRoofRouteMonteCarloHorizonSeconds;
		config.combat.stepSeconds = kMonteCarloStepSeconds;
		config.combat.plantDecisionInterval = kMonteCarloPlantDecisionSeconds;
		config.combat.terminalBlockedSecondUtility =
			kMonteCarloTerminalBlockedSecondUtility;
		config.combat.terminalBlockedSecondsCap =
			kMonteCarloTerminalBlockedSecondsCap;
		config.hijackerZombieId = GetNightRoofHijackerID();
		config.hijackerExecutionSeconds = config.hijackerZombieId != NULL_ZOMBIE_ID
			? warningSeconds : -1.0f;
		config.survivalMode = mIsSurvival;
		config.survivalExecutionLineCap =
			static_cast<float>(kNightRoofHijackerSurvivalLineCap);
		config.guideImmunitySeconds = kGroundingZombieControlImmunityDuration;
		config.guideImmunityRadius = kGroundingZombieControlImmunityRadius;

		for (const int plantID : mEntityRegistry.GetAllPlantIDs()) {
			const Plant* plant = mEntityRegistry.GetPlant(plantID);
			if (!plant || !plant->IsActive() || plant->IsPreview()
				|| plant->IsSquished() || plant->GetSleepState()
				|| plant->mPlantType != PlantType::PLANT_ICESHROOM
				|| plant->GetCurrentTrackName() != "anim_idle") continue;
			const float currentFrame = plant->GetCurrentFrame();
			const float speed = plant->GetAnimationSpeed();
			if (currentFrame >= 16.0f || speed <= 0.0f) continue;
			config.pendingControlEvents.push_back({
				plantID,
				std::max(0.0f, (16.0f - currentFrame) / (12.0f * speed)),
				20.0f,
				20.0f,
				4.0f,
				6.0f
			});
		}

		std::uint32_t seed = 2166136261u;
		auto mixSeed = [&seed](std::uint32_t value) {
			seed ^= value;
			seed *= 16777619u;
		};
		mixSeed(static_cast<std::uint32_t>(mBoardFrame));
		mixSeed(static_cast<std::uint32_t>(mCurrentWave));
		mixSeed(static_cast<std::uint32_t>(mNightRoofHijackerID));
		NightRoofChargeResult result;
		{
			PROFILE_SCOPE("MC.NightRoofRoute.Rollouts");
			result = PlantDefenseMonteCarlo::ChooseNightRoofChargeRoute(
				snapshot, candidates, config, seed);
		}
		mNightRoofChargeRouteStats.rolloutCount = result.rolloutCount;
		mNightRoofChargeRouteStats.candidateCount =
			static_cast<int>(candidates.size());
		mNightRoofChargeRouteStats.sampledZombieCount = result.sampledZombieCount;
		mNightRoofChargeRouteStats.sampledPlantCount = result.sampledPlantCount;
		mNightRoofChargeRouteStats.supportPlantCount = result.supportPlantCount;
		mNightRoofChargeRouteStats.cardCount = result.cardCount;
		mNightRoofChargeRouteStats.bestScore = result.score;
		if (result.candidateIndex >= 0
			&& result.candidateIndex < static_cast<int>(candidates.size())) {
			const NightRoofChargeCandidate& chosen = candidates[result.candidateIndex];
			mNightRoofChargeRow = chosen.row;
			mNightRoofChargeGuided = chosen.guided;
			mNightRoofChargeGuideID = chosen.guided
				? chosen.guideZombieId : NULL_ZOMBIE_ID;
			mNightRoofChargeRouteUsedMonteCarlo = true;
			recordDecisionTime();
			return;
		}
	}

	// 推演不可用时才消费正式 RNG：有接地候选就只在稳定 ID 候选集中均匀选，否则随机普通行。
	if (!guides.empty()) {
		const int index = GameRandom::Range(0, static_cast<int>(guides.size()) - 1);
		const std::shared_ptr<Zombie>& guide = guides[static_cast<std::size_t>(index)];
		if (guide) {
			mNightRoofChargeRow = guide->mRow;
			mNightRoofChargeGuided = true;
			mNightRoofChargeGuideID = guide->mZombieID;
		}
	} else {
		mNightRoofChargeRow = GameRandom::Range(0, mRows - 1);
	}
	recordDecisionTime();
}

/**
 * 统一接收雨势与自然闪电增量。积累阶段跨过 100 的同一笔增量保留溢出；
 * 预警和放电演出期间的新增电荷也只进入余电，不改变本次已公开的放电强度。
 */
void Board::AddNightRoofCharge(float amount)
{
	if (!SupportsNightRoofCharge() || amount <= 0.0f
		|| !std::isfinite(amount)) return;
	if (mNightRoofChargePhase == NightRoofChargePhase::CHARGING) {
		const float combinedCharge = mNightRoofCharge + amount;
		mNightRoofCharge = std::clamp(combinedCharge,
			0.0f, kNightRoofChargeMaximum);
		if (!mNightRoofHijackerSelectionAttempted
			&& combinedCharge >= kNightRoofHijackerLockThreshold) {
			TryLockNightRoofHijacker();
		}
		if (combinedCharge >= kNightRoofChargeMaximum) {
			const float overflow = combinedCharge - kNightRoofChargeMaximum;
			BeginNightRoofChargeWarning();
			mNightRoofOvercharge = std::clamp(
				mNightRoofOvercharge + overflow,
				0.0f, kNightRoofOverchargeMaximum);
		}
		return;
	}
	mNightRoofOvercharge = std::clamp(mNightRoofOvercharge + amount,
		0.0f, kNightRoofOverchargeMaximum);
}

int Board::GetNightRoofExecutionLine() const
{
	const HijackerZombie* hijacker = GetValidNightRoofHijacker();
	if (!hijacker) return 0;
	const int currentHealth = hijacker->GetCountableExecutionHealth();
	return mIsSurvival
		? std::min(currentHealth, kNightRoofHijackerSurvivalLineCap)
		: currentHealth;
}

bool Board::IsZombieThreatenedByNightRoofHijacker(const Zombie* zombie) const
{
	const int line = GetNightRoofExecutionLine();
	return line > 0 && zombie && zombie->mZombieID != mNightRoofHijackerID
		&& !zombie->IsPreview() && zombie->IsActive() && !zombie->IsDying()
		&& zombie->GetCountableExecutionHealth() > 0
		&& zombie->GetCountableExecutionHealth() <= line;
}

/** 普通层与南瓜壳合并计血；承载层永远不属于处决组，飞行覆盖层只随宿主一并清除。 */
bool Board::IsPlantThreatenedByNightRoofHijacker(const Plant* plant) const
{
	const int line = GetNightRoofExecutionLine();
	if (line <= 0 || !plant || !plant->IsActive() || plant->IsPreview()
		|| plant->IsSquished() || plant->mRow < 0 || plant->mRow >= mRows
		|| plant->mColumn < 0 || plant->mColumn >= mColumns) return false;
	const Cell* cell = mCells[plant->mRow][plant->mColumn];
	if (!cell || plant->mPlantID == cell->GetUnderPlantID()) return false;
	if (plant->mPlantID != cell->GetNormalPlantID()
		&& plant->mPlantID != cell->GetPumpkinPlantID()
		&& plant->mPlantID != cell->GetOverlayPlantID()) return false;
	if (GetNightRoofHijackerSupportProtector(plant)) return false;

	int64_t groupHealth = 0;
	bool hasHostLayer = false;
	for (const int id : { cell->GetNormalPlantID(), cell->GetPumpkinPlantID() }) {
		const Plant* member = mEntityRegistry.GetPlant(id);
		if (!member || !member->IsActive() || member->IsPreview()
			|| member->IsSquished()) continue;
		hasHostLayer = true;
		groupHealth += std::max(0, member->mPlantHealth);
	}
	return hasHostLayer && groupHealth > 0 && groupHealth <= line;
}

Plant* Board::GetNightRoofChargeSupportProtector(const Plant* target) const
{
	if (!target) return nullptr;
	const PlantFootprint footprint = GetPlantFootprint(target->mPlantType);
	for (std::size_t i = 0; i < footprint.count; ++i) {
		Plant* support = GetUnderPlantAt(
			target->mRow + footprint.cells[i].rowOffset,
			target->mColumn + footprint.cells[i].columnOffset);
		if (support && support->IsActive() && support->mPlantHealth > 0
			&& !support->IsPreview() && !support->IsSquished()
			&& support->ProtectsSupportedPlantFromNightRoofCharge(target)) {
			return support;
		}
	}
	return nullptr;
}

Plant* Board::GetNightRoofHijackerSupportProtector(const Plant* target) const
{
	if (!target) return nullptr;
	const PlantFootprint footprint = GetPlantFootprint(target->mPlantType);
	for (std::size_t i = 0; i < footprint.count; ++i) {
		Plant* support = GetUnderPlantAt(
			target->mRow + footprint.cells[i].rowOffset,
			target->mColumn + footprint.cells[i].columnOffset);
		if (support && support->IsActive() && support->mPlantHealth > 0
			&& !support->IsPreview() && !support->IsSquished()
			&& support->ProtectsSupportedPlantFromNightRoofHijacker(target)) {
			return support;
		}
	}
	return nullptr;
}

float Board::GetNightRoofHijackerPulseAlpha() const
{
	if (!GetValidNightRoofHijacker()) return 0.0f;
	const float wave = 0.5f + 0.5f * std::sin(
		static_cast<float>(mBoardFrame) * (mNightRoofHijackerFinalizing ? 0.55f : 0.20f));
	return mNightRoofHijackerFinalizing
		? 125.0f + 90.0f * wave : 45.0f + 75.0f * wave;
}

void Board::CancelNightRoofHijacker(int zombieID)
{
	if (zombieID == NULL_ZOMBIE_ID || zombieID != mNightRoofHijackerID) return;
	auto* hijacker = dynamic_cast<HijackerZombie*>(mEntityRegistry.GetZombie(zombieID));
	mNightRoofHijackerID = NULL_ZOMBIE_ID;
	mNightRoofHijackerFinalizing = false;
	if (hijacker) hijacker->ClearNightRoofLock();
}

void Board::ResetNightRoofHijackerCycle()
{
	if (auto* hijacker = dynamic_cast<HijackerZombie*>(
		mEntityRegistry.GetZombie(mNightRoofHijackerID))) {
		hijacker->ClearNightRoofLock();
	}
	mNightRoofHijackerSelectionAttempted = false;
	mNightRoofHijackerID = NULL_ZOMBIE_ID;
	mNightRoofHijackerWarningExtended = false;
	mNightRoofHijackerFinalizing = false;
}

/**
 * 目标先按格子和稳定 ID 完整快照，再统一死亡。这样植物槽位释放、施法者掉头和特殊僵尸
 * 死亡回调都不能改变本次集合；气球额外生命不在 GetCountableExecutionHealth 中。
 */
void Board::ResolveNightRoofHijackerExecution()
{
	HijackerZombie* caster = GetValidNightRoofHijacker();
	const int line = GetNightRoofExecutionLine();
	if (!caster || line <= 0) return;

	std::unordered_set<int> plantTargetIDs;
	std::unordered_set<int> protectedSupportIDs;
	std::unordered_set<int> processedNormalPlantIDs;
	for (int row = 0; row < mRows; ++row) {
		for (int column = 0; column < mColumns; ++column) {
			const Cell* cell = mCells[row][column];
			if (!cell) continue;
			const int normalID = cell->GetNormalPlantID();
			if (normalID != NULL_PLANT_ID
				&& !processedNormalPlantIDs.insert(normalID).second) continue;
			int64_t groupHealth = 0;
			std::vector<int> group;
			for (const int id : { cell->GetNormalPlantID(), cell->GetPumpkinPlantID() }) {
				Plant* plant = mEntityRegistry.GetPlant(id);
				if (!plant || !plant->IsActive() || plant->IsPreview()
					|| plant->IsSquished()) continue;
				groupHealth += std::max(0, plant->mPlantHealth);
				group.push_back(id);
			}
			if (group.empty() || groupHealth <= 0 || groupHealth > line) continue;
			if (Plant* protector = GetNightRoofHijackerSupportProtector(
				mEntityRegistry.GetPlant(group.front()))) {
				protectedSupportIDs.insert(protector->mPlantID);
				continue;
			}
			plantTargetIDs.insert(group.begin(), group.end());
			if (Plant* overlay = mEntityRegistry.GetPlant(cell->GetOverlayPlantID());
				overlay && overlay->IsActive() && !overlay->IsPreview()) {
				plantTargetIDs.insert(overlay->mPlantID);
			}
		}
	}
	std::vector<int> plantTargets(plantTargetIDs.begin(), plantTargetIDs.end());
	std::sort(plantTargets.begin(), plantTargets.end());

	std::vector<int> zombieTargets;
	for (const int id : mEntityRegistry.GetAllZombieIDs()) {
		Zombie* zombie = mEntityRegistry.GetZombie(id);
		if (!zombie || id == caster->mZombieID || zombie->IsPreview()
			|| !zombie->IsActive() || zombie->IsDying()) continue;
		const int health = zombie->GetCountableExecutionHealth();
		if (health > 0 && health <= line) zombieTargets.push_back(id);
	}
	std::sort(zombieTargets.begin(), zombieTargets.end());

	// 成功释放从这一提交边沿开始封锁本波余下候选，并跳过后续两个完整波次。
	BeginHijackerSpawnCooldown();
	// 先解除 Board 交叉引用，施法者掉头粒子和死亡回调便不会取消或重入这次批处理。
	mNightRoofHijackerID = NULL_ZOMBIE_ID;
	mNightRoofHijackerFinalizing = false;
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("HijackerElectricFlash", caster->GetVisualPosition());
	}
	// 不按目标数量叠音；只在有效处决快照真正提交时给一次全场听觉落点。
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HIJACKER_EXECUTE,
		kNightRoofHijackerExecuteVolume);
	caster->TakeHijackerExecution();
	std::vector<int> sortedProtectedSupportIDs(
		protectedSupportIDs.begin(), protectedSupportIDs.end());
	std::sort(sortedProtectedSupportIDs.begin(), sortedProtectedSupportIDs.end());
	for (const int supportID : sortedProtectedSupportIDs) {
		if (Plant* support = mEntityRegistry.GetPlant(supportID)) {
			support->OnNightRoofChargeProtectionTriggered();
		}
	}

	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_EXPLOSION, 0.7f);

	for (const int id : plantTargets) {
		Plant* plant = mEntityRegistry.GetPlant(id);
		if (!plant || !plant->IsActive()) continue;
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("JackExplode", plant->GetVisualPosition());
		}
		plant->Die();
	}
	for (const int id : zombieTargets) {
		Zombie* zombie = mEntityRegistry.GetZombie(id);
		if (!zombie || !zombie->IsActive() || zombie->IsDying()) continue;
		if (g_particleSystem) {
			g_particleSystem->EmitEffect("HijackerElectricFlash", zombie->GetVisualPosition() + 
				Vector(23.0f, 27.0f));
		}
		zombie->TakeHijackerExecution();
	}
}

/**
 * 放电只在 WARNING 转入 DISCHARGING 的边沿结算一次。植物先按稳定 ID 冻结
 * 接地保护分配，引导路线再按放电时位置发放编队免控，普通路线中的僵尸随后消费
 * 同一批仍有效的接地范围，最后统一结算植物反噬；
 * 读档恢复 DISCHARGING 不会重复伤害。
 */
void Board::ResolveNightRoofChargeDischarge()
{
	if (!SupportsNightRoofCharge() || mNightRoofChargeRow < 0
		|| mNightRoofChargeRow >= mRows) return;

	const int row = mNightRoofChargeRow;
	const bool wetRow = IsRoofRunoffFlowing() && IsRoofRunoffRowSelected(row);
	std::vector<int> zombieIDs = mEntityRegistry.GetAllZombieIDs();
	std::sort(zombieIDs.begin(), zombieIDs.end());
	if (mNightRoofChargeGuided) {
		Zombie* guide = mEntityRegistry.GetZombie(mNightRoofChargeGuideID);
		if (guide && guide->CanGuideNightRoofCharge()) {
			auto getColliderCenter = [](const Zombie* zombie) {
				Vector center = zombie->GetPosition();
				if (const ColliderComponent* collider = zombie->GetColliderComponent()) {
					const SDL_FRect bounds = collider->GetBoundingBox();
					center = Vector(bounds.x + bounds.w * 0.5f,
						bounds.y + bounds.h * 0.5f);
				}
				return center;
			};
			const Vector guideCenter = getColliderCenter(guide);
			const float radiusSquared = kGroundingZombieControlImmunityRadius
				* kGroundingZombieControlImmunityRadius;
			// 放电边沿冻结稳定 ID 快照；同阵营目标一经获得免控，随后走出范围也保留余时。
			for (const int id : zombieIDs) {
				Zombie* target = mEntityRegistry.GetZombie(id);
				if (!target || !target->IsActive() || target->IsDying()
					|| target->IsPreview()
					|| target->IsMindControlled() != guide->IsMindControlled()) {
					continue;
				}
				const Vector targetCenter = getColliderCenter(target);
				const float dx = targetCenter.x - guideCenter.x;
				const float dy = targetCenter.y - guideCenter.y;
				if (dx * dx + dy * dy > radiusSquared) continue;
				target->GrantControlImmunity(kGroundingZombieControlImmunityMask,
					kGroundingZombieControlImmunityDuration, true);
			}
			Vector anchor;
			if (g_particleSystem && guide->TryGetNightRoofChargeGuideAnchor(anchor)) {
				g_particleSystem->EmitEffect("GroundingZombieLightning", anchor);
			}
		}
	}
	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	std::vector<int> groundingProviderIDs;
	std::unordered_set<int> groundingProviderSet;
	std::unordered_set<int> protectedSupportIDs;
	float zombieDamageMultiplier = 1.0f;
	for (int column = 0; column < mColumns; ++column) {
		Plant* support = GetUnderPlantAt(row, column);
		if (!support || !support->IsActive() || support->IsPreview()
			|| support->mPlantHealth <= 0 || support->IsSquished()) {
			continue;
		}
		const float multiplier = support->GetNightRoofChargeZombieDamageMultiplier();
		zombieDamageMultiplier = std::max(zombieDamageMultiplier, multiplier);
		if (multiplier > 1.0f) protectedSupportIDs.insert(support->mPlantID);
	}
	for (const int id : plantIDs) {
		Plant* plant = mEntityRegistry.GetPlant(id);
		if (!plant || !plant->IsActive() || plant->IsPreview()
			|| plant->IsSquished() || plant->IsBungeeTargeted()
			|| plant->mRow != row || plant->IsRoofSupportPlant()) {
			continue;
		}
		if (Plant* support = GetNightRoofChargeSupportProtector(plant)) {
			protectedSupportIDs.insert(support->mPlantID);
			continue;
		}

		Plant* groundingProvider = nullptr;
		int nearestColumnDistance = std::numeric_limits<int>::max();
		for (const int providerID : plantIDs) {
			Plant* candidate = mEntityRegistry.GetPlant(providerID);
			if (!candidate || !candidate->CanGroundNightRoofChargeFor(plant)) continue;
			const int columnDistance = std::abs(candidate->mColumn - plant->mColumn);
			// plantIDs 已排序；同距时保留先遇到的较小稳定 ID。
			if (columnDistance < nearestColumnDistance) {
				nearestColumnDistance = columnDistance;
				groundingProvider = candidate;
			}
		}
		if (groundingProvider) {
			if (groundingProviderSet.insert(groundingProvider->mPlantID).second) {
				groundingProviderIDs.push_back(groundingProvider->mPlantID);
			}
			continue;
		}

		const bool onWetSlope = wetRow && plant->mColumn >= 0
			&& plant->mColumn < kRoofSlopeColumnCount;
		plant->ApplyShutdown(onWetSlope
			? kNightRoofWetPlantShutdownDuration
			: kNightRoofPlantShutdownDuration);
	}
	for (const int id : zombieIDs) {
		if (mNightRoofChargeGuided) break;
		Zombie* zombie = mEntityRegistry.GetZombie(id);
		if (!zombie || !zombie->IsActive() || zombie->IsDying()
			|| zombie->mRow != row
			|| !zombie->CanBeAffectedByGroundHazards()) {
			continue;
		}
		const bool onWetSlope = wetRow
			&& zombie->GetPosition().x <= GetRoofSlopeEndX();
		const int baseDamage = static_cast<int>(std::lround((onWetSlope
			? kNightRoofWetZombieDamage : kNightRoofZombieDamage)
			* zombieDamageMultiplier));
		const float paralysisDuration = onWetSlope
			? kNightRoofWetZombieParalysisDuration
			: kNightRoofZombieParalysisDuration;

		// 绝缘者按距离、再按稳定 ID 选出唯一承接者。自身若可承接以零距离优先；
		// 湿润绝缘者会拒绝承接，随后走它自己的湿坡 360 点胸甲结算。
		Zombie* protector = nullptr;
		float nearestDistance = std::numeric_limits<float>::max();
		for (const int providerID : zombieIDs) {
			Zombie* candidate = mEntityRegistry.GetZombie(providerID);
			if (!candidate || !candidate->CanProtectFromNightRoofCharge(zombie)) continue;
			const float distance = std::abs(
				candidate->GetPosition().x - zombie->GetPosition().x);
			if (distance < nearestDistance) {
				nearestDistance = distance;
				protector = candidate;
			}
		}
		if (protector && protector->AbsorbNightRoofChargeFor(zombie, baseDamage)) {
			continue;
		}
		zombie->TakeNightRoofChargeImpact(
			baseDamage, paralysisDuration, onWetSlope);
	}

	std::vector<int> sortedProtectedSupportIDs(
		protectedSupportIDs.begin(), protectedSupportIDs.end());
	std::sort(sortedProtectedSupportIDs.begin(), sortedProtectedSupportIDs.end());
	for (const int supportID : sortedProtectedSupportIDs) {
		if (Plant* support = mEntityRegistry.GetPlant(supportID)) {
			support->OnNightRoofChargeProtectionTriggered();
		}
	}

	// 同次放电的植物与僵尸效果已经全部冻结；现在反噬死亡不会改变本次接地结果。
	std::sort(groundingProviderIDs.begin(), groundingProviderIDs.end());
	for (const int providerID : groundingProviderIDs) {
		Plant* provider = mEntityRegistry.GetPlant(providerID);
		if (!provider) continue;
		const bool onWetSlope = wetRow && provider->mColumn >= 0
			&& provider->mColumn < kRoofSlopeColumnCount;
		provider->AbsorbGroundedNightRoofCharge(onWetSlope);
	}
}

bool Board::IsNightRoofChargeProtectionSuppressed(const Zombie* zombie) const
{
	if (!zombie) return false;
	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	for (const int id : plantIDs) {
		const Plant* plant = mEntityRegistry.GetPlant(id);
		if (plant && plant->SuppressesNightRoofChargeProtectionFor(zombie)) {
			return true;
		}
	}
	return false;
}

/**
 * 黑夜屋顶雷荷与径流并行推进。预警转放电时按一次快照结算实体效果；
 * 活动阶段新增电荷只截留为下一轮余电，避免改写已公开的本次强度或重复命中。
 */
void Board::UpdateNightRoofCharge(float deltaTime)
{
	if (!SupportsNightRoofCharge()) {
		ResetNightRoofHijackerCycle();
		mNightRoofCharge = 0.0f;
		mNightRoofOvercharge = 0.0f;
		mNightRoofChargePhase = NightRoofChargePhase::CHARGING;
		mNightRoofChargePhaseTimer = 0.0f;
		mNightRoofChargeRow = -1;
		mNightRoofChargeGuided = false;
		mNightRoofChargeGuideID = NULL_ZOMBIE_ID;
		return;
	}
	if (deltaTime <= 0.0f) return;

	float chargeDelta = 0.0f;
	switch (mRainIntensity) {
	case RainIntensity::CLEAR:
		chargeDelta = -kNightRoofChargeClearLeakPerSecond;
		break;
	case RainIntensity::LIGHT:
		chargeDelta = kNightRoofChargeLightPerSecond;
		break;
	case RainIntensity::MEDIUM:
		chargeDelta = kNightRoofChargeMediumPerSecond;
		break;
	case RainIntensity::HEAVY:
		chargeDelta = kNightRoofChargeHeavyPerSecond;
		break;
	}
	chargeDelta += GetNightRoofHijackerRainChargeBonusPerSecond();

	if (mNightRoofChargePhase == NightRoofChargePhase::WARNING
		|| mNightRoofChargePhase == NightRoofChargePhase::DISCHARGING) {
		// 活动阶段只截留正向输入；晴夜泄漏要等余电兑现为下一轮主电荷后才重新生效。
		if (chargeDelta > 0.0f) AddNightRoofCharge(chargeDelta * deltaTime);
		if (mNightRoofChargePhase == NightRoofChargePhase::WARNING
			&& mNightRoofHijackerID != NULL_ZOMBIE_ID
			&& !GetValidNightRoofHijacker()) {
			CancelNightRoofHijacker(mNightRoofHijackerID);
		}
		mNightRoofChargePhaseTimer = std::max(0.0f,
			mNightRoofChargePhaseTimer - deltaTime);
		if (mNightRoofChargePhase == NightRoofChargePhase::WARNING
			&& !mNightRoofHijackerFinalizing
			&& mNightRoofChargePhaseTimer <= kNightRoofHijackerFinalDuration) {
			if (HijackerZombie* hijacker = GetValidNightRoofHijacker()) {
				mNightRoofHijackerFinalizing = true;
				hijacker->BeginNightRoofFinalization();
			}
		}
		if (mNightRoofChargePhaseTimer > 0.0f) return;

		if (mNightRoofChargePhase == NightRoofChargePhase::WARNING) {
			mNightRoofChargePhase = NightRoofChargePhase::DISCHARGING;
			mNightRoofChargePhaseTimer = kNightRoofChargeDischargeDuration;
			ResolveNightRoofHijackerExecution();
			ResolveNightRoofChargeDischarge();
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_THUNDER,
				kThunderSoundVolume);
			return;
		}

		mNightRoofCharge = mNightRoofOvercharge;
		mNightRoofOvercharge = 0.0f;
		mNightRoofChargePhase = NightRoofChargePhase::CHARGING;
		mNightRoofChargePhaseTimer = 0.0f;
		mNightRoofChargeRow = -1;
		mNightRoofChargeGuided = false;
		mNightRoofChargeGuideID = NULL_ZOMBIE_ID;
		ResetNightRoofHijackerCycle();
		return;
	}

	if (chargeDelta > 0.0f) {
		AddNightRoofCharge(chargeDelta * deltaTime);
	}
	else {
		mNightRoofCharge = std::clamp(mNightRoofCharge
			+ chargeDelta * deltaTime, 0.0f, kNightRoofChargeMaximum);
	}
}

/** 校验并恢复黑夜屋顶雷荷；损坏组合和其他背景都回到中性积累状态。 */
void Board::RestoreNightRoofChargeState(float charge, NightRoofChargePhase phase,
	int row, float phaseTimer, float overcharge, bool hijackerSelectionAttempted,
	int hijackerID, bool hijackerWarningExtended, bool hijackerFinalizing,
	bool guided, int guideID)
{
	mNightRoofCharge = 0.0f;
	mNightRoofOvercharge = 0.0f;
	mNightRoofChargePhase = NightRoofChargePhase::CHARGING;
	mNightRoofChargePhaseTimer = 0.0f;
	mNightRoofChargeRow = -1;
	mNightRoofChargeGuided = false;
	mNightRoofChargeGuideID = NULL_ZOMBIE_ID;
	mNightRoofChargeRouteUsedMonteCarlo = false;
	mNightRoofChargeRouteStats = {};
	mNightRoofChargeRouteDecisionMicros = 0;
	mNightRoofHijackerSelectionAttempted = false;
	mNightRoofHijackerID = NULL_ZOMBIE_ID;
	mNightRoofHijackerWarningExtended = false;
	mNightRoofHijackerFinalizing = false;
	if (!SupportsNightRoofCharge() || !std::isfinite(charge)
		|| !std::isfinite(phaseTimer) || !std::isfinite(overcharge)) return;

	mNightRoofCharge = std::clamp(charge, 0.0f, kNightRoofChargeMaximum);
	mNightRoofHijackerSelectionAttempted = hijackerSelectionAttempted;
	mNightRoofHijackerID = hijackerSelectionAttempted
		? hijackerID : NULL_ZOMBIE_ID;
	if (phase == NightRoofChargePhase::CHARGING) return;
	if ((phase != NightRoofChargePhase::WARNING
		&& phase != NightRoofChargePhase::DISCHARGING)
		|| row < 0 || row >= mRows || phaseTimer < 0.0f) {
		mNightRoofCharge = 0.0f;
		ResetNightRoofHijackerCycle();
		return;
	}
	mNightRoofCharge = kNightRoofChargeMaximum;
	mNightRoofChargePhase = phase;
	mNightRoofHijackerWarningExtended = phase == NightRoofChargePhase::WARNING
		&& hijackerWarningExtended;
	mNightRoofHijackerFinalizing = mNightRoofHijackerWarningExtended
		&& hijackerFinalizing && phaseTimer <= kNightRoofHijackerFinalDuration;
	mNightRoofChargePhaseTimer = std::clamp(phaseTimer, 0.0f,
		phase == NightRoofChargePhase::WARNING
			? (mNightRoofHijackerWarningExtended
				? kNightRoofHijackerWarningDuration : kNightRoofChargeWarningDuration)
			: kNightRoofChargeDischargeDuration);
	mNightRoofChargeRow = row;
	mNightRoofChargeGuided = guided;
	mNightRoofChargeGuideID = guided ? guideID : NULL_ZOMBIE_ID;
	mNightRoofOvercharge = std::clamp(overcharge,
		0.0f, kNightRoofOverchargeMaximum);
	if (phase == NightRoofChargePhase::DISCHARGING) {
		mNightRoofHijackerID = NULL_ZOMBIE_ID;
		mNightRoofHijackerFinalizing = false;
	}
}

float Board::GetNightRoofHijackerRainChargeBonusPerSecond() const
{
	if (mRainIntensity == RainIntensity::CLEAR
		|| !mEntityRegistry.HasActiveNightRoofHijacker()) return 0.0f;
	return kNightRoofHijackerRainChargeBonusPerSecond;
}

void Board::FinalizeNightRoofHijackerLoad()
{
	if (mNightRoofHijackerID == NULL_ZOMBIE_ID) return;
	HijackerZombie* hijacker = GetValidNightRoofHijacker();
	if (!hijacker || mNightRoofChargePhase == NightRoofChargePhase::DISCHARGING) {
		CancelNightRoofHijacker(mNightRoofHijackerID);
		return;
	}
	const bool warning = mNightRoofChargePhase == NightRoofChargePhase::WARNING;
	hijacker->RestoreNightRoofPhase(
		true, warning && mNightRoofHijackerFinalizing, warning);
}

void Board::FinalizeIceStatueExecutionerLoad()
{
	std::vector<int> zombieIDs = mEntityRegistry.GetAllZombieIDs();
	std::sort(zombieIDs.begin(), zombieIDs.end());
	for (const int zombieID : zombieIDs) {
		if (auto* executioner = dynamic_cast<IceStatueExecutionerZombie*>(
			mEntityRegistry.GetZombie(zombieID))) {
			executioner->FinalizeIceSealLoad();
		}
	}

	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	for (const int plantID : plantIDs) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (!plant || !plant->IsIceSealed()) continue;
		const int ownerID = plant->GetIceSealOwnerZombieID();
		auto* executioner = dynamic_cast<IceStatueExecutionerZombie*>(
			mEntityRegistry.GetZombie(ownerID));
		if (!executioner || !executioner->OwnsIceSealFor(plantID)
			|| !IsPlantFootprintFrozen(
				plant->mPlantType, plant->mRow, plant->mColumn)) {
			plant->ReleaseIceSeal(ownerID);
		}
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
	mPolarTemperatureC = LerpWeatherValue(
		mPolarStartTemperatureC, mPolarTargetTemperatureC, eased);
	mPolarHumidityPercent = LerpWeatherValue(
		mPolarStartHumidityPercent, mPolarTargetHumidityPercent, eased);
	mPolarWindSpeedMps = LerpWeatherValue(
		mPolarStartWindSpeedMps, mPolarTargetWindSpeedMps, eased);
	if (mPolarFluctuationTimer >= mPolarFluctuationDuration) {
		BeginPolarGaugeFluctuation();
	}
}

/** 一次性锁定连续曲线的全部随机量，运行阶段只做插值。 */
void Board::RollNextPolarNightPlan()
{
	if (!SupportsPolarNightEnvironment()) return;
	const int levelInArea = AdventureProgression::GetLevelNumberInArea(mLevel);
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
	const int levelInArea = AdventureProgression::GetLevelNumberInArea(mLevel);
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

/**
 * 正式波次只在活动雪穴处建立延迟事务；预算与行选择已经由调用方提交，
 * 因而预警期间封穴只能把这一只改回右侧入口，不能取消或重抽僵尸。
 */
bool Board::CreateOrQueueWaveZombie(ZombieType actualType, int row, float rightEdgeX,
	bool tutorialSnowBurrow)
{
	if (SupportsPolarNightEnvironment() && row >= 0
		&& row < static_cast<int>(mSnowHoles.size())
		&& mSnowHoles[row].phase == SnowHolePhase::ACTIVE) {
		const int holeColumn = mSnowHoles[row].column;
		const bool warningAlreadyVisible = std::any_of(
			mPendingSnowHoleSpawns.begin(), mPendingSnowHoleSpawns.end(),
			[row, holeColumn](const PendingSnowHoleSpawn& pending) {
				return pending.row == row && pending.holeColumn == holeColumn;
			});
		mPendingSnowHoleSpawns.push_back({ actualType, row, holeColumn,
			mCurrentWave, kPolarHoleSpawnWarningSeconds, tutorialSnowBurrow });
		if (!warningAlreadyVisible && g_particleSystem) {
			g_particleSystem->EmitEffect("SnowHolePuff",
				GetCellCenterPosition(row, holeColumn));
		}
		if (!warningAlreadyVisible) {
			AudioSystem::PlaySound(
				ResourceKeys::Sounds::SOUND_SNOW_PEA_SPARKLES, 0.35f);
		}
		return true;
	}

	Zombie* zombie = CreateResolvedWaveZombie(actualType, row, rightEdgeX);
	if (!zombie) return false;
	AssignMistFuelReward(zombie);
	return true;
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

		const bool useHole = it->row >= 0
			&& it->row < static_cast<int>(mSnowHoles.size())
			&& mSnowHoles[it->row].phase == SnowHolePhase::ACTIVE
			&& mSnowHoles[it->row].column == it->holeColumn;
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
	mPolarWindParticleTimer = LerpWeatherValue(
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
		|| AdventureProgression::GetLevelNumberInArea(mLevel) != 9) return;
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
		mPolarTemperatureC = LerpWeatherValue(
			mPolarStartTemperatureC, mPolarTargetTemperatureC, eased);
		mPolarHumidityPercent = LerpWeatherValue(
			mPolarStartHumidityPercent, mPolarTargetHumidityPercent, eased);
		mPolarWindSpeedMps = LerpWeatherValue(
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
			mPolarWhiteoutTimer = AdventureProgression::GetLevelNumberInArea(mLevel) == 9
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

void Board::TrySpawnAdaptiveHelmetTutorialWave()
{
	if (mIsSurvival || mLevel != kAdaptiveHelmetTutorialLevel
		|| mAdaptiveHelmetTutorialWaveSpawned
		|| !mPolarFirstWhiteoutCompleted) {
		return;
	}

	const int row = SelectSpawnRow(ZombieType::ZOMBIE_ADAPTIVE_HELMET);
	if (row < 0) return;
	const ZombieType actualType = ResolveWaveZombieType(
		ZombieType::ZOMBIE_ADAPTIVE_HELMET);
	if (actualType == ZombieType::NUM_ZOMBIE_TYPES) return;

	Zombie* zombie = CreateResolvedWaveZombie(actualType, row,
		static_cast<float>(SCENE_WIDTH) + 40.0f);
	if (!zombie) {
		--mAdaptiveHelmetsSpawnedThisWave;
		return;
	}
	mAdaptiveHelmetTutorialWaveSpawned = true;
	UpdateZombieMetrics();
}

void Board::UpdateWeather(float deltaTime)
{
	if (!mWeatherInitialized || deltaTime <= 0.0f) return;
	UpdatePolarNightEnvironment(deltaTime);
	UpdateWinterTemperature(deltaTime);
	// 场景积累器只由背景资格决定；通用天气的冒险进度门槛不能反向关闭屋顶固有机制。
	UpdateRoofRunoff(deltaTime);
	UpdateNightRoofCharge(deltaTime);
	if (!SupportsWeather()) return;
	if (IsStormyNightActive()) {
		EnforceStormyNightWeather();
		UpdateWeatherTransition(deltaTime);
		if (!IsRainEffectEmitting()) {
			mRainVisualActive = false;
			EmitRainEffect(kStormyNightLockedDuration);
		}
		if (!IsWinterPrecipitationSnow()) UpdateRainGroundSplash(deltaTime);
		UpdateTyphoon(deltaTime);
		UpdateStormyNightFlash(deltaTime);
		return;
	}
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
	if (mRainIntensity != RainIntensity::CLEAR && mWeatherTimer > 0.0f
		&& !IsWinterPrecipitationSnow()) {
		UpdateRainGroundSplash(deltaTime);
	}
	if (mRainIntensity == RainIntensity::HEAVY && mWeatherTimer > 0.0f) {
		UpdateTyphoon(deltaTime);
		if (!IsWinterPrecipitationSnow()) {
			mLightningTimer -= deltaTime;
			if (mLightningTimer <= 0.0f) {
				TriggerLightning();
				mLightningTimer = GameRandom::Range(kLightningRepeatMin, kLightningRepeatMax);
			}
		}
		else {
			mLightningTimer = 0.0f;
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
	mWeatherForecastDisrupted = false;
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
		if (mPresentation && !IsWeatherPanelInterferenceActive()) {
			mPresentation->ShowCurrentWeatherNotice();
		}
		return;
	}
	BeginRain(intensity, std::max(duration, 0.1f), canIntensify, true, false);
	FinishWeatherTransitionImmediately();
}

bool Board::SetFogWeatherForTesting(FogWeatherIntensity intensity, float duration)
{
	if (!SupportsFogWeather()) return false;
	mFogWeatherInitialized = true;
	mFogDispersal = 0.0f;
	mFogVisualOffsetX = 0.0f;
	ClearFogWeatherForecast();
	if (intensity == FogWeatherIntensity::DEFAULT) {
		EndFogWeather(std::max(duration, 0.1f));
	}
	else {
		BeginFogWeather(intensity, std::max(duration, 0.1f));
	}
	return mFogWeatherIntensity == intensity;
}

bool Board::SetFogWeatherForecastForTesting(FogWeatherIntensity forecast,
	FogWeatherIntensity actual, float revealIn)
{
	if (!SupportsFogWeather() || !mFogWeatherInitialized) return false;
	mForecastFogWeatherIntensity = forecast;
	mActualForecastFogWeatherIntensity = actual;
	mFogWeatherForecastReady = true;
	mFogWeatherForecastDisrupted = false;
	mFogWeatherTimer = std::max(revealIn, 0.1f);
	return true;
}

bool Board::SetFogDispersalForTesting(float dispersal)
{
	if (!SupportsStageFog() || !mFogWeatherInitialized
		|| !std::isfinite(dispersal)) return false;
	mFogDispersal = std::clamp(dispersal, 0.0f, 1.0f);
	return true;
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
	mWeatherForecastDisrupted = false;
	mWeatherTimer = std::max(revealIn, 0.1f);
	PreparePendingHeavyTyphoon();
	return true;
}

/** 用确定性等级覆盖大雨预报警报或真实台风初态；NONE 同样表示已锁定。 */
bool Board::SetPendingHeavyTyphoonForTesting(TyphoonStrength strength, int promptVariant)
{
	if (!NeedsPendingHeavyForecastState()
		|| strength < TyphoonStrength::NONE || strength > TyphoonStrength::SUPER
		|| promptVariant < -1 || promptVariant > 2) {
		return false;
	}
	mPendingHeavyTyphoonPrepared = true;
	mPendingHeavyTyphoonOpeningProtected = false;
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
	if (mRainIntensity != RainIntensity::HEAVY || IsWinterPrecipitationSnow()) return false;
	TriggerLightning();
	return true;
}

bool Board::SetTyphoonForTesting(TyphoonStrength strength, WindDirection direction,
	float gustIn, float directionIn, int gustsRemaining, float decayIn)
{
	if (mRainIntensity != RainIntensity::HEAVY) return false;
	if (!SupportsTyphoon() && strength != TyphoonStrength::NONE) return false;
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
	if (!SupportsTyphoon() || mRainIntensity != RainIntensity::HEAVY
		|| chanceRoll < 1 || chanceRoll > 100
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

bool Board::SetRoofRunoffForTesting(float charge, RoofRunoffPhase phase,
	int rowMask, float phaseTimer, float retainedCharge)
{
	if (!SupportsRoofRunoff() || !std::isfinite(charge)
		|| !std::isfinite(phaseTimer) || !std::isfinite(retainedCharge)) return false;
	if (phase == RoofRunoffPhase::WARNING || phase == RoofRunoffPhase::FLOWING) {
		const int validRowMask = mRows > 0 ? (1 << mRows) - 1 : 0;
		if ((rowMask & validRowMask) == 0 || (rowMask & ~validRowMask) != 0) return false;
		if (phaseTimer <= 0.0f) {
			phaseTimer = phase == RoofRunoffPhase::WARNING
				? kRoofRunoffWarningDuration : kRoofRunoffFlowDuration;
		}
	}
	else {
		rowMask = 0;
		phaseTimer = 0.0f;
		retainedCharge = 0.0f;
	}
	RestoreRoofRunoffState(charge, phase, rowMask, phaseTimer, retainedCharge);
	return mRoofRunoffPhase == phase;
}

bool Board::SetNightRoofChargeForTesting(float charge, NightRoofChargePhase phase,
	int row, float phaseTimer, float overcharge)
{
	if (!SupportsNightRoofCharge() || !std::isfinite(charge)
		|| !std::isfinite(phaseTimer) || !std::isfinite(overcharge)) return false;
	if (phase == NightRoofChargePhase::WARNING
		|| phase == NightRoofChargePhase::DISCHARGING) {
		if (row < 0 || row >= mRows) return false;
		if (phaseTimer <= 0.0f) {
			phaseTimer = phase == NightRoofChargePhase::WARNING
				? kNightRoofChargeWarningDuration
				: kNightRoofChargeDischargeDuration;
		}
	}
	else {
		row = -1;
		phaseTimer = 0.0f;
		overcharge = 0.0f;
	}
	// AutoTest 可能在同一局中重设雷荷；参数校验后才解除旧锁定，失败调用不得改变局面。
	ResetNightRoofHijackerCycle();
	RestoreNightRoofChargeState(charge, phase, row, phaseTimer, overcharge,
		false, NULL_ZOMBIE_ID, false, false, false, NULL_ZOMBIE_ID);
	return mNightRoofChargePhase == phase;
}

void Board::TriggerRainGroundSplashForTesting()
{
	TriggerRainGroundSplash();
}

bool Board::IsPoolBackground() const
{
	return mBackGround == Background::WATER_POOL
		|| mBackGround == Background::NIGHT_WATER_POOL;
}

bool Board::IsRoofBackground() const
{
	return mBackGround == Background::ROOF
		|| mBackGround == Background::NIGHT_ROOF;
}

float Board::GetRoofSlopeEndX() const
{
	return CELL_INITALIZE_POS_X
		+ static_cast<float>(kRoofSlopeColumnCount) * CELL_COLLIDER_SIZE_X;
}

/**
 * 把 C# 屋顶的连续坡面换算为当前 1100 宽场景的 Board 网格坐标。
 * 平台段保持行中心不变，房屋侧按 1:4 坡度向屏幕下方抬升。
 */
float Board::GetRowCenterYAtX(int row, float worldX) const
{
	if (row < 0 || row >= mRows) return -1.0f;

	float centerY = mCellInitialY + static_cast<float>(row) * mCellHeight
		+ mCellHeight * 0.5f;
	if (IsRoofBackground()) {
		centerY += std::max(0.0f, GetRoofSlopeEndX() - worldX)
			* kRoofSlopeRisePerPixel;
	}
	return centerY;
}

float Board::GetMowerTerrainY(int row, float worldX) const
{
	const float centerY = GetRowCenterYAtX(row, worldX);
	if (centerY < 0.0f) return centerY;
	return centerY + (IsRoofBackground() ? kRoofMowerTerrainOffsetY : -3.0f);
}

bool Board::SupportsWeather() const
{
	// 基础天气保留唯一的进度门槛：正式一大关不启用；所有无尽地形均独立启用。
	if (SupportsPolarNightEnvironment()) return false;
	if (mIsSurvival) return true;
	return AdventureProgression::IsAdventureLevel(mLevel)
		&& AdventureProgression::GetAreaNumber(mLevel) >= 2;
}

bool Board::SupportsRoofRunoff() const
{
	return IsRoofBackground();
}

bool Board::SupportsNightRoofCharge() const
{
	return mBackGround == Background::NIGHT_ROOF;
}

float Board::GetRoofRunoffZombieDriftVelocity(int row, float worldX) const
{
	if (!IsRoofRunoffFlowing() || !IsRoofRunoffRowSelected(row)
		|| worldX > GetRoofSlopeEndX()) return 0.0f;
	return kRoofRunoffZombieDriftSpeed;
}

int Board::GetRoofRunoffRowCount() const
{
	int count = 0;
	for (int row = 0; row < mRows; ++row) {
		if (IsRoofRunoffRowSelected(row)) ++count;
	}
	return count;
}

float Board::GetRoofRunoffFlowProgress() const
{
	if (!IsRoofRunoffFlowing()) return 0.0f;
	return std::clamp(1.0f - mRoofRunoffPhaseTimer / kRoofRunoffFlowDuration,
		0.0f, 1.0f);
}

float Board::GetNightRoofChargeDischargeProgress() const
{
	if (!IsNightRoofChargeDischarging()) return 0.0f;
	return std::clamp(1.0f
		- mNightRoofChargePhaseTimer / kNightRoofChargeDischargeDuration,
		0.0f, 1.0f);
}

bool Board::SupportsStageFog() const
{
	// 第四大关继续由背景提供通用雾场；其他背景的固定关卡统一由冒险进度表登记。
	return mBackGround == Background::NIGHT_WATER_POOL
		|| AdventureProgression::HasLevelSpecificFogMechanics(mLevel);
}

/** 统一判定玩家是否允许当前地图生成或保留台风。 */
bool Board::SupportsTyphoon() const
{
	return SupportsWeather()
		&& GameAPP::GetInstance().mTyphoonWeatherEnabled
		&& mBackGround != Background::WINTER_GARDEN;
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

bool Board::HasDisruptibleWeatherForecast() const
{
	return HasWeatherForecast() || HasColdWaveForecast() || HasFogWeatherForecast();
}

bool Board::SupportsWeatherPanelInterference() const
{
	return SupportsWeather() || SupportsWinterTemperature() || SupportsFogWeather();
}

bool Board::CanBeginWeatherPanelInterference() const
{
	return SupportsWeatherPanelInterference()
		&& mWeatherPanelInterferenceTimer < kMaximumWeatherPanelInterferenceDuration;
}

float Board::GetMaximumWeatherPanelInterferenceDuration() const
{
	return kMaximumWeatherPanelInterferenceDuration;
}

int Board::DisruptWeatherForecastPanel()
{
	int disruptedMask = 0;
	if (HasWeatherForecast()) {
		if (mPresentation) mPresentation->CancelHeavyRainWarning();
		mWeatherForecastDisrupted = true;
		disruptedMask |= 1;
	}
	if (DisruptColdWaveForecast()) disruptedMask |= 2;
	if (HasFogWeatherForecast()) {
		mFogWeatherForecastDisrupted = true;
		disruptedMask |= 4;
	}
	return disruptedMask;
}

/**
 * 向当前剩余时长追加一个有界整栏黑障；bit3 区分“成功补时但当帧尚无公开预报”的有效提交。
 */
int Board::BeginWeatherPanelInterference(float duration)
{
	if (!CanBeginWeatherPanelInterference() || !std::isfinite(duration)
		|| duration <= 0.0f) return 0;
	mWeatherPanelInterferenceTimer = std::min(
		kMaximumWeatherPanelInterferenceDuration,
		mWeatherPanelInterferenceTimer + duration);
	const int disruptedMask = DisruptWeatherForecastPanel();
	if (mPresentation) {
		mPresentation->CancelHeavyRainWarning();
		mPresentation->CancelWeatherInformationNotices();
	}
	return disruptedMask | 8;
}

/**
 * 黑障期间每帧在雨雪、寒潮和雾势都完成推进后截获新广播，再按游戏时间递减窗口。
 */
void Board::UpdateWeatherPanelInterference(float deltaTime)
{
	if (!IsWeatherPanelInterferenceActive()) return;
	DisruptWeatherForecastPanel();
	mWeatherPanelInterferenceTimer = std::max(
		0.0f, mWeatherPanelInterferenceTimer - std::max(0.0f, deltaTime));
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

Plant* Board::SelectIceStatueExecutionTarget(
	int sourceZombieID, float strikeInterval, int strikeDamage,
	MonteCarloTargetStats* stats)
{
	if (!SupportsWinterTemperature()) return nullptr;
	if (stats) *stats = {};
	const auto& gameData = GameDataManager::GetInstance();
	const int backlineColumnCount = (mColumns + 1) / 2;
	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	std::vector<int> eligiblePlantIDs;
	std::vector<int> requiredStrikeCounts;
	Plant* best = nullptr;
	float bestValue = -1.0f;
	for (const int plantID : plantIDs) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (!plant || !plant->IsActive() || plant->IsPreview()
			|| plant->IsSquished() || plant->IsBungeeTargeted()
			|| plant->IsIceSealed() || plant->mPlantHealth <= 0
			|| !IsPlantFootprintFrozen(
				plant->mPlantType, plant->mRow, plant->mColumn)) {
			continue;
		}
		const Cell* cell = mCells[plant->mRow][plant->mColumn];
		if (!cell || (cell->GetNormalPlantID() != plantID
			&& cell->GetPumpkinPlantID() != plantID)) {
			continue;
		}
		const PlantSimulationProfile& profile =
			gameData.GetPlantSimulationProfile(plant->mPlantType);
		if (!profile.persistent || profile.supportOnly) continue;
		eligiblePlantIDs.push_back(plantID);
		requiredStrikeCounts.push_back(
			plant->GetIceExecutionRequiredStrikeCount());
		float value = static_cast<float>(
			gameData.GetPlantSunCost(plant->mPlantType));
		if (profile.sunPerSecond > 0.0f) {
			value += kMonteCarloSunProducerFutureValue;
		}
		if (plant->mColumn < backlineColumnCount) {
			value *= kMonteCarloBacklineMultiplier;
		}
		// ID 已升序；相同价值不替换，稳定保留较小 ID。
		if (!best || value > bestValue) {
			best = plant;
			bestValue = value;
		}
	}
	if (GameAPP::GetInstance().mEnableMonteCarloAI
		&& !eligiblePlantIDs.empty()) {
		int selectedPlantID = NULL_PLANT_ID;
		if (PickMonteCarloPlantRemovalTarget(
			eligiblePlantIDs, sourceZombieID, selectedPlantID, stats,
			strikeInterval, strikeDamage, &requiredStrikeCounts)) {
			if (Plant* selected = mEntityRegistry.GetPlant(selectedPlantID)) {
				return selected;
			}
		}
	}
	return best;
}

bool Board::TryPreventIceExecutionSeal(Plant& target) const
{
	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	for (const int plantID : plantIDs) {
		Plant* provider = mEntityRegistry.GetPlant(plantID);
		if (!provider || !provider->IsActive() || provider->IsSquished()
			|| provider->IsBungeeTargeted() || provider->IsIceSealed()
			|| provider->IsShutdown()
			|| std::abs(provider->mRow - target.mRow) > 1
			|| std::abs(provider->mColumn - target.mColumn) > 1) {
			continue;
		}
		if (provider->TryPreventIceExecutionSealFor(&target)) return true;
	}
	return false;
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

bool Board::SupportsPlanternMechanics() const
{
	if (!SupportsStageFog()) return false;
	// 4-1 仍只教学基础雾；固定复用关卡始终包含路灯花燃料、照明、索敌与雾火闭环。
	return AdventureProgression::HasLevelSpecificFogMechanics(mLevel)
		|| AdventureProgression::GetLevelNumberInArea(mLevel) >= 2;
}

bool Board::SupportsFogWeather() const
{
	// 雾势沿用基础雾场的单一门禁；雨势与雾势仍保持两个独立开关。
	return SupportsStageFog();
}

/** 按九关制关内进度换算原版覆盖曲线；6-9 复用末关基准，便于集中调难。 */
int Board::GetBaseFogLeftColumn() const
{
	if (!SupportsStageFog()) return mColumns;
	const int levelInArea = AdventureProgression::GetLevelNumberInArea(mLevel);
	if (levelInArea <= 1) return std::min(6, mColumns - 1);
	if (levelInArea <= 6) return std::min(5, mColumns - 1);
	return std::min(4, mColumns - 1);
}

int Board::GetFogLayerCount() const
{
	if (!SupportsStageFog()) return 0;
	switch (mFogWeatherIntensity) {
	case FogWeatherIntensity::DEFAULT: return 1;
	case FogWeatherIntensity::SMALL:  return 2;
	case FogWeatherIntensity::NORMAL: return 2;
	case FogWeatherIntensity::DENSE:  return 3;
	}
	return 1;
}

int Board::GetEffectiveFogLeftColumn() const
{
	const int baseColumn = GetBaseFogLeftColumn();
	if (!SupportsStageFog()) return baseColumn;
	int expansion = 0;
	switch (mFogWeatherIntensity) {
	case FogWeatherIntensity::DEFAULT: break;
	case FogWeatherIntensity::SMALL:  expansion = kSmallFogColumnExpansion; break;
	case FogWeatherIntensity::NORMAL: expansion = kNormalFogColumnExpansion; break;
	case FogWeatherIntensity::DENSE:  expansion = kDenseFogColumnExpansion; break;
	}
	return std::max(0, baseColumn - expansion);
}

float Board::GetFogCellAlpha(int row, int col) const
{
	if (row < 0 || row >= GetFogDrawRowCount()
		|| col < 0 || col >= mColumns) return 0.0f;
	const int index = row * mColumns + col;
	return index < static_cast<int>(mFogCellAlpha.size())
		? mFogCellAlpha[index] : 0.0f;
}

bool Board::IsZombieObscuredByFog(const Zombie* zombie) const
{
	if (!zombie || !SupportsPlanternMechanics() || mColumns <= 0) return false;
	const int column = std::clamp(static_cast<int>(
		(zombie->GetPosition().x - CELL_INITALIZE_POS_X) / CELL_COLLIDER_SIZE_X),
		0, mColumns - 1);
	const int row = std::clamp(zombie->mRow, 0, mRows - 1);
	return GetFogCellAlpha(row, column) > kFogTargetingAlphaThreshold;
}

Plantern* Board::GetActivePlantern() const
{
	Plant* plant = mEntityRegistry.GetPlant(mActivePlanternID);
	auto* plantern = dynamic_cast<Plantern*>(plant);
	return plantern && !plantern->IsSquished() ? plantern : nullptr;
}

float Board::GetPlanternIllumination(int row, int col) const
{
	if (!SupportsPlanternMechanics()) return 0.0f;
	const Plantern* plantern = GetActivePlantern();
	if (!plantern || !plantern->HasUsableLight()) return 0.0f;

	const int relativeX = col - plantern->mColumn;
	const int relativeY = row - plantern->mRow;
	const int dx = std::abs(relativeX);
	const int dy = std::abs(relativeY);
	switch (plantern->GetGear()) {
	case PlanternGear::OFF:
		return 0.0f;
	case PlanternGear::LOW:
		return relativeX >= -kPlanternLowBackRadius
			&& relativeX <= kPlanternLowFrontRadius
			&& dy <= kPlanternLowVerticalRadius
			? 1.0f : 0.0f;
	case PlanternGear::MEDIUM:
		return (dx <= kPlanternMediumBaseRadiusX
				&& dy <= kPlanternMediumVerticalRadius
				&& dx + dy <= kPlanternMediumManhattanLimit)
			|| (relativeX == kPlanternMediumFrontExtension
				&& dy <= kPlanternMediumFrontHalfHeight)
			? 1.0f : 0.0f;
	case PlanternGear::HIGH: {
		const auto isInsideHighShape = [](int x, int y) {
			const int shapeDx = std::abs(x);
			const int shapeDy = std::abs(y);
			return (shapeDx <= kPlanternHighBaseRadiusX
					&& shapeDy <= kPlanternHighVerticalRadius
					&& shapeDx + shapeDy <= kPlanternHighManhattanLimit)
				|| (x == kPlanternHighFrontExtension
					&& shapeDy <= kPlanternHighFrontHalfHeight);
		};
		if (!isInsideHighShape(relativeX, relativeY)) return 0.0f;
		// 由四邻域实时识别扩展后轮廓，只有真正最外圈保留薄雾。
		const bool isOuterEdge = !isInsideHighShape(relativeX - 1, relativeY)
			|| !isInsideHighShape(relativeX + 1, relativeY)
			|| !isInsideHighShape(relativeX, relativeY - 1)
			|| !isInsideHighShape(relativeX, relativeY + 1);
		return isOuterEdge ? kPlanternHighEdgeIllumination : 1.0f;
	}
	}
	return 0.0f;
}

bool Board::CanPlantAcquireZombie(const Plant* plant, const Zombie* zombie) const
{
	if (!plant || !zombie || !plant->CanAcquireZombie(zombie)) return false;
	if (IsPolarSnowBlindActive()) {
		auto centerOf = [](const GameObject* object,
			const ColliderComponent* collider) {
			if (!object) return Vector::zero();
			if (!collider) return object->GetTransform()
				? object->GetTransform()->GetPosition() : Vector::zero();
			const SDL_FRect bounds = collider->GetBoundingBox();
			return Vector(bounds.x + bounds.w * 0.5f, bounds.y + bounds.h * 0.5f);
		};
		const Vector plantCenter = centerOf(plant, plant->GetColliderComponent());
		const Vector zombieCenter = centerOf(zombie, zombie->GetColliderComponent());
		if ((zombieCenter - plantCenter).sqrMagnitude()
			> kPolarSnowBlindRange * kPolarSnowBlindRange) return false;
	}
	if (!SupportsPlanternMechanics()) return true;
	const Vector plantPosition = plant->GetPosition();
	const Vector zombiePosition = zombie->GetPosition();
	if (std::abs(zombie->mRow - plant->mRow) <= 1
		&& std::abs(zombiePosition.x - plantPosition.x)
			<= kFogCloseDetectionRange) {
		return true;
	}

	const int column = std::clamp(static_cast<int>(
		(zombiePosition.x - CELL_INITALIZE_POS_X) / CELL_COLLIDER_SIZE_X),
		0, mColumns - 1);
	const int row = std::clamp(zombie->mRow, 0, mRows - 1);
	if (GetFogCellAlpha(row, column) <= kFogTargetingAlphaThreshold) return true;

	// 当前格仍被浓雾遮挡时，只允许从植物一侧已经看清的边界再深入一格。
	if (zombiePosition.x == plantPosition.x) return false;
	const int directionTowardPlant = zombiePosition.x > plantPosition.x ? -1 : 1;
	for (int step = 1; step <= kFogTargetingMarginColumns; ++step) {
		const int adjacentColumn = column + directionTowardPlant * step;
		if (adjacentColumn < 0 || adjacentColumn >= mColumns) break;
		if (GetFogCellAlpha(row, adjacentColumn) <= kFogTargetingAlphaThreshold) {
			return true;
		}
	}
	return false;
}

float Board::GetPlanternSunProductionMultiplier(const Plant* producer) const
{
	if (!producer) return 1.0f;
	const Plantern* plantern = GetActivePlantern();
	if (!plantern || !plantern->HasUsableLight()) return 1.0f;
	const float illumination = GetPlanternIllumination(producer->mRow, producer->mColumn);
	if (illumination <= 0.0f) return 1.0f;

	float peak = 1.0f;
	switch (plantern->GetGear()) {
	case PlanternGear::OFF: break;
	case PlanternGear::LOW: peak = 1.10f; break;
	case PlanternGear::MEDIUM: peak = 1.20f; break;
	case PlanternGear::HIGH: peak = 1.35f; break;
	}
	return 1.0f + (peak - 1.0f) * illumination;
}

float Board::GetPlanternFuel() const
{
	const Plantern* plantern = GetActivePlantern();
	return plantern ? plantern->GetFuel() : 0.0f;
}

float Board::GetPlanternFuelRatio() const
{
	const Plantern* plantern = GetActivePlantern();
	return plantern ? plantern->GetFuelRatio() : 0.0f;
}

int Board::GetPlanternGearValue() const
{
	const Plantern* plantern = GetActivePlantern();
	return plantern ? static_cast<int>(plantern->GetGear()) : 0;
}

float Board::GetPlanternFuelFullHintTimer() const
{
	const Plantern* plantern = GetActivePlantern();
	return plantern ? plantern->GetFuelFullHintTimer() : 0.0f;
}

void Board::SetPlanternGear(PlanternGear gear)
{
	if (Plantern* plantern = GetActivePlantern()) plantern->SetGear(gear);
}

void Board::NotifyPlanternRemoved(int plantID)
{
	if (mActivePlanternID == plantID) mActivePlanternID = NULL_PLANT_ID;
}

void Board::TogglePlanternGearMenu()
{
	if (mPresentation && GetActivePlantern()) {
		mPresentation->TogglePlanternGearMenu();
	}
}

void Board::CollectMistFuelFromZombie(Zombie* zombie)
{
	if (!zombie || !SupportsPlanternMechanics()
		|| zombie->IsMindControlled()) return;
	const float reward = zombie->ClaimMistFuelReward()
		* static_cast<float>(mPerkManager.GetMistFuelMultiplier());
	if (reward <= 0.0f) return;

	Plantern* plantern = GetActivePlantern();
	if (!plantern) return;
	const float accepted = plantern->ReserveFuel(reward);
	if (accepted <= 0.0f) return;

	GameObjectManager::GetInstance().CreateGameObject<MistFuel>(
		LAYER_EFFECTS, this,
		zombie->GetVisualPosition() + Vector(0.0f, -24.0f),
		plantern->mPlantID, accepted);
}

void Board::RelayZombieDeathWard(const Zombie* source)
{
	if (!source || !mPerkManager.HasZombieDeathRelay()
		|| source->IsMindControlled()) return;
	Zombie* recipient = nullptr;
	for (int zombieID : mEntityRegistry.GetAllZombieIDs()) {
		Zombie* candidate = mEntityRegistry.GetZombie(zombieID);
		if (!candidate || candidate == source || !candidate->IsActive()
			|| candidate->IsDying() || candidate->IsMindControlled()
			|| candidate->mRow != source->mRow) continue;
		if (!recipient || candidate->GetPosition().x < recipient->GetPosition().x
			|| (candidate->GetPosition().x == recipient->GetPosition().x
				&& candidate->mZombieID < recipient->mZombieID)) {
			recipient = candidate;
		}
	}
	if (recipient) recipient->mFreeHitsRemaining = std::max(
		recipient->mFreeHitsRemaining, 1);
}

bool Board::SetPlanternFuelForTesting(float fuel)
{
	if (!std::isfinite(fuel) || fuel < 0.0f) return false;
	Plantern* plantern = GetActivePlantern();
	if (!plantern) return false;
	plantern->SetFuel(fuel);
	return true;
}

bool Board::AwardPlanternFuelForTesting(float amount)
{
	if (!std::isfinite(amount) || amount <= 0.0f) return false;
	Plantern* plantern = GetActivePlantern();
	if (!plantern) return false;
	plantern->AddFuel(amount);
	return true;
}

int Board::GetFogTileVariant(int row, int col) const
{
	// 模拟原版 mGridCelLook 的逐格随机外观，但保持读档稳定且不消费玩法随机数。
	unsigned hash = 2166136261u;
	auto mix = [&hash](unsigned value) {
		hash ^= value;
		hash *= 16777619u;
		hash ^= hash >> 13;
	};
	mix(static_cast<unsigned>(std::max(mLevel, 0)));
	mix(static_cast<unsigned>(std::max(row, 0)));
	mix(static_cast<unsigned>(std::max(col, 0)));
	hash ^= hash >> 16;
	hash *= 0x7feb352du;
	hash ^= hash >> 15;
	return static_cast<int>(hash % 8u);
}

Vector Board::GetFogTilePosition(int row, int col) const
{
	return Vector(
		CELL_INITALIZE_POS_X + static_cast<float>(col) * CELL_COLLIDER_SIZE_X
			- kFogTileLeftOverlap + mFogVisualOffsetX,
		mCellInitialY + static_cast<float>(row) * mCellHeight - kFogTileTopOverlap);
}

int Board::GetVisibleFogCellCount() const
{
	return static_cast<int>(std::count_if(
		mFogCellAlpha.begin(), mFogCellAlpha.end(),
		[](float alpha) { return alpha >= 1.0f; }));
}

int Board::GetMaximumFogAlpha() const
{
	if (mFogCellAlpha.empty()) return 0;
	return static_cast<int>(std::lround(*std::max_element(
		mFogCellAlpha.begin(), mFogCellAlpha.end())));
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

/** 返回指定类型能否被正式选行逻辑放进水路；无水路状态机的品种在此集中排除。 */
bool Board::CanZombieTypeSpawnInPool(ZombieType type) const
{
	switch (type) {
	case ZombieType::ZOMBIE_ZAMBONI:
	case ZombieType::ZOMBIE_GILDED_ZAMBONI:
	case ZombieType::ZOMBIE_CATAPULT:
	case ZombieType::ZOMBIE_ELITE_CATAPULT:
	case ZombieType::ZOMBIE_POGO:
	case ZombieType::ZOMBIE_ELITE_POGO:
	case ZombieType::ZOMBIE_DIGGER:
	case ZombieType::ZOMBIE_ELITE_DIGGER:
	case ZombieType::ZOMBIE_BOBSLED_TEAM:
	case ZombieType::NUM_ZOMBIE_TYPES:
		return false;
	default:
		return true;
	}
}

Vector Board::GetCellCenterPosition(int row, int col) const
{
	const float cellLeftX = CELL_INITALIZE_POS_X
		+ static_cast<float>(col) * CELL_COLLIDER_SIZE_X;
	float centerY = mCellInitialY + static_cast<float>(row) * mCellHeight
		+ mCellHeight * 0.5f;
	if (IsRoofBackground()) {
		// 格子沿用原版按列离散抬升；僵尸则通过 GetRowCenterYAtX 沿同一坡面连续移动。
		centerY += std::max(0.0f, GetRoofSlopeEndX() - cellLeftX)
			* kRoofSlopeRisePerPixel;
	}
	return Vector(cellLeftX + CELL_COLLIDER_SIZE_X * 0.5f, centerY);
}

void Board::ExtendIceTrail(int row, float frontX)
{
	if (row < 0 || row >= mRows || row >= static_cast<int>(mIceTimer.size())
		|| IsPoolRow(row)) return;

	// 原版 Zamboni 在屋顶把冰道左缘钳在坡顶平台；斜坡既不结冰，也不参与禁种判定。
	const float leftLimit = IsRoofBackground() ? GetRoofSlopeEndX() : kIceTrailLeftLimit;
	const float clampedFront = std::max(frontX, leftLimit);
	mIceMinX[row] = std::min(mIceMinX[row], clampedFront);
	// 车辆还在屏幕右侧入场时就激活冰道；左缘钳在屏幕右边界，进入画面后自然连续增长。
	mIceTimer[row] = kIceTrailDuration;
}

void Board::ExtendGoldenIceTrail(int row, float frontX)
{
	if (row < 0 || row >= mRows || row >= static_cast<int>(mGoldenIceTimer.size())
		|| IsPoolRow(row)) return;

	const float leftLimit = IsRoofBackground() ? GetRoofSlopeEndX() : kIceTrailLeftLimit;
	const float clampedFront = std::max(frontX, leftLimit);
	mGoldenIceMinX[row] = std::min(mGoldenIceMinX[row], clampedFront);
	mGoldenIceTimer[row] = kGoldenIceTrailDuration;
}

void Board::ShortenIceTrail(int row, float maxRemainingSeconds)
{
	if (row < 0 || row >= mRows || row >= static_cast<int>(mIceTimer.size())
		|| row >= static_cast<int>(mGoldenIceTimer.size())) return;
	const float limit = std::max(0.0f, maxRemainingSeconds);
	if (mIceTimer[row] > 0.0f) {
		mIceTimer[row] = std::min(mIceTimer[row], limit);
	}
	if (mGoldenIceTimer[row] > 0.0f) {
		mGoldenIceTimer[row] = std::min(mGoldenIceTimer[row], limit);
	}
}

bool Board::IsIceAt(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns
		|| row >= static_cast<int>(mIceTimer.size())
		|| row >= static_cast<int>(mGoldenIceTimer.size())) return false;
	auto trailCovers = [this, col](float minX, float timer) {
		if (timer <= 0.0f) return false;
		const int startCol = std::clamp(static_cast<int>(std::floor(
			(minX + kIceTrailGridProbeOffset - CELL_INITALIZE_POS_X)
			/ CELL_COLLIDER_SIZE_X)), 0, mColumns - 1);
		return col >= startCol;
	};
	return trailCovers(mIceMinX[row], mIceTimer[row])
		|| trailCovers(mGoldenIceMinX[row], mGoldenIceTimer[row]);
}

bool Board::IsGoldenIceAtWorld(int row, float worldX) const
{
	if (row < 0 || row >= mRows || row >= static_cast<int>(mGoldenIceTimer.size())
		|| mGoldenIceTimer[row] <= 0.0f) return false;
	return worldX >= mGoldenIceMinX[row] && worldX <= GetIceTrailRightX();
}

float Board::GetIceTrailMinX(int row) const
{
	return row >= 0 && row < mRows && row < static_cast<int>(mIceMinX.size())
		? mIceMinX[row] : 0.0f;
}

float Board::GetIceTrailTimeRemaining(int row) const
{
	return row >= 0 && row < mRows && row < static_cast<int>(mIceTimer.size())
		? mIceTimer[row] : 0.0f;
}

float Board::GetGoldenIceTrailMinX(int row) const
{
	return row >= 0 && row < mRows && row < static_cast<int>(mGoldenIceMinX.size())
		? mGoldenIceMinX[row] : 0.0f;
}

float Board::GetGoldenIceTrailTimeRemaining(int row) const
{
	return row >= 0 && row < mRows && row < static_cast<int>(mGoldenIceTimer.size())
		? mGoldenIceTimer[row] : 0.0f;
}

float Board::GetIceTrailRightX() const
{
	return static_cast<float>(SCENE_WIDTH);
}

void Board::UpdateIceTrails(float deltaTime)
{
	if (deltaTime <= 0.0f || mBoardState != BoardState::GAME) return;
	for (int row = 0; row < mRows && row < static_cast<int>(mIceTimer.size()); ++row) {
		if (mIceTimer[row] > 0.0f) {
			mIceTimer[row] = std::max(0.0f, mIceTimer[row] - deltaTime);
		}
		if (mIceTimer[row] <= 0.0f) {
			mIceMinX[row] = GetIceTrailRightX();
		}
		if (mGoldenIceTimer[row] > 0.0f) {
			mGoldenIceTimer[row] = std::max(0.0f, mGoldenIceTimer[row] - deltaTime);
		}
		if (mGoldenIceTimer[row] <= 0.0f) {
			mGoldenIceMinX[row] = GetIceTrailRightX();
		}
	}
}

void Board::DrawIceTrails(Graphics* g) const
{
	if (!g) return;
	auto& resources = ResourceManager::GetInstance();
	const Texture* iceBody = resources.GetTexture(ResourceKeys::Textures::IMAGE_ICE, false);
	const Texture* iceCap = resources.GetTexture(ResourceKeys::Textures::IMAGE_ICE_CAP, false);
	const Texture* goldenBody = resources.GetTexture(
		ResourceKeys::Textures::IMAGE_GOLDEN_ICE, false);
	const Texture* goldenCap = resources.GetTexture(
		ResourceKeys::Textures::IMAGE_GOLDEN_ICE_CAP, false);

	const float boardRight = GetIceTrailRightX();
	auto drawTrail = [this, g](int row, float minX, float remaining, float rightX,
		const Texture* body, const Texture* cap) {
		if (remaining <= 0.0f || rightX <= minX + 0.01f || !body || !cap
			|| body->width <= 0 || body->height <= 0) return;
		const float alpha = 255.0f * std::clamp(
			remaining / kIceTrailFadeDuration, 0.0f, 1.0f);
		const glm::vec4 tint(255.0f, 255.0f, 255.0f, alpha);
		// 原版固定取最右侧平地行高；屋顶冰道因此始终水平，且不会沿斜坡弯折。
		const float drawY = GetRowCenterYAtX(row, GetIceTrailRightX())
			- mCellHeight * 0.5f + kIceTrailTopOffset;
		const float bodyStart = minX + kIceTrailCapBodyOverlap;
		const float length = std::max(0.0f, rightX - bodyStart);
		float drawX = bodyStart;
		float firstWidth = std::fmod(length, static_cast<float>(body->width));
		if (firstWidth > 0.01f) {
			g->DrawTextureRegion(body,
				static_cast<float>(body->width) - firstWidth, 0.0f,
				firstWidth, static_cast<float>(body->height),
				drawX, drawY, firstWidth, static_cast<float>(body->height),
				0.0f, tint);
			drawX += firstWidth;
		}
		while (drawX < rightX - 0.01f) {
			const float width = std::min(static_cast<float>(body->width), rightX - drawX);
			g->DrawTextureRegion(body, 0.0f, 0.0f, width,
				static_cast<float>(body->height), drawX, drawY, width,
				static_cast<float>(body->height), 0.0f, tint);
			drawX += width;
		}
		g->DrawTexture(cap, minX, drawY,
			static_cast<float>(cap->width), static_cast<float>(cap->height),
			0.0f, tint);
	};

	for (int row = 0; row < mRows && row < static_cast<int>(mIceTimer.size()); ++row) {
		const bool goldenActive = mGoldenIceTimer[row] > 0.0f;
		// 普通冰道只画到黄色冰道左缘；重叠段完全交给黄色材质，避免半透明蓝底串色。
		const float ordinaryRight = goldenActive
			? std::clamp(mGoldenIceMinX[row], mIceMinX[row], boardRight)
			: boardRight;
		drawTrail(row, mIceMinX[row], mIceTimer[row], ordinaryRight, iceBody, iceCap);
		drawTrail(row, mGoldenIceMinX[row], mGoldenIceTimer[row],
			boardRight, goldenBody, goldenCap);
	}
}

void Board::InitializeCell(int rows, int cols)
{
	mRows = rows + 1;
	mColumns = cols + 1;
	if (IsPoolBackground()) {
		mCellInitialY = kPoolCellInitialY;
		mCellHeight = kPoolCellHeight;
	}
	else if (IsRoofBackground()) {
		mCellInitialY = CELL_INITALIZE_POS_Y + kRoofCellInitialYOffsetY;
		mCellHeight = kRoofCellHeight;
	}
	else {
		mCellInitialY = CELL_INITALIZE_POS_Y;
		mCellHeight = CELL_COLLIDER_SIZE_Y;
	}
	mCells.resize(mRows);
	for (int i = 0; i < mRows; i++)
	{
		mCells[i].resize(mColumns);
		for (int j = 0; j < mColumns; j++)
		{
			const Vector center = GetCellCenterPosition(i, j);
			Vector position(center.x - CELL_COLLIDER_SIZE_X * 0.5f,
				center.y - mCellHeight * 0.5f);
			Cell* cell = GameObjectManager::GetInstance().CreateGameObject<Cell>(
				LAYER_BACKGROUND, i, j, position,
				Vector(CELL_COLLIDER_SIZE_X, mCellHeight));
			mCells[i][j] = cell;
		}
	}
}

void Board::CreateBoom(const Vector& position, int plantRow, int damage)
{
	g_particleSystem->EmitEffect("CherryBomb", position);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CHERRYBOMB, 0.4f);
	ShakeBoard(3.0f, -4.0f);   // 原版 ShakeBoard(3,-4)：0.12s 单次弹跳

	// 水路僵尸的 Transform 含美术下沉，纵向命中必须使用僵尸与植物的逻辑行。
	for (int row = plantRow - 1; row <= plantRow + 1; ++row) {
		mEntityRegistry.ForEachZombieInRow(row, [&](Zombie* zombie) {
			if (zombie->IsMindControlled()) return;
			if (std::abs(zombie->GetPosition().x - position.x) <= 130.0f) {
				// 统一灰烬入口内部决定化灰或数值扣血；特殊僵尸可拒绝化灰并限制每次灰烬伤害。
				zombie->TakePlantAshDamage(damage);
			}
		});
	}
	// 原版对僵尸使用圆形命中，但扶梯另按爆心格的 3x3 方形范围清除。
	RemoveLaddersInBlastSquare(position, plantRow, 1);
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

bool Board::TryGetNightRoofChargeGuideAnchor(Vector& anchor) const
{
	if (!mNightRoofChargeGuided || mNightRoofChargeGuideID == NULL_ZOMBIE_ID) {
		return false;
	}
	const Zombie* guide = mEntityRegistry.GetZombie(mNightRoofChargeGuideID);
	return guide && guide->TryGetNightRoofChargeGuideAnchor(anchor);
}

void Board::CreateDoomBoom(const Vector& position, int plantRow, int damage)
{
	g_particleSystem->EmitEffect("Doom", position);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_DOOMSHROOM, 0.5f);
	// 比樱桃更剧烈：双倍振幅 + 0.5s 衰减正弦来回甩 5 个半周期（原版两者同为 3,-4，主人要求毁灭菇加强）
	ShakeBoard(6.0f, -9.0f, 0.5f, 5);
	std::vector<int> zombieIDs = mEntityRegistry.GetAllZombieIDs();
	for (auto zombieID : zombieIDs)
	{
		if (auto zombie = mEntityRegistry.GetZombie(zombieID)) {
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
	// 毁灭菇沿用原版 rowRange=3，清除爆心格周围 7x7 方形范围内的扶梯。
	RemoveLaddersInBlastSquare(position, plantRow, 3);
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

Ladder* Board::AddLadder(int row, int column, LadderStyle style)
{
	if (row < 0 || row >= mRows || column < 0 || column >= mColumns) return nullptr;
	if (Ladder* existing = GetLadderAt(row, column)) return existing;
	auto ladder = GameObjectManager::GetInstance().CreateGameObjectAsShared<Ladder>(
		LAYER_GAME_ZOMBIE, this, row, column, style);
	if (ladder) mLadders.push_back(ladder);
	return ladder.get();
}

Ladder* Board::GetLadderAt(int row, int column)
{
	Ladder* found = nullptr;
	mLadders.erase(std::remove_if(mLadders.begin(), mLadders.end(),
		[&](const std::weak_ptr<Ladder>& weak) {
			auto ladder = weak.lock();
			if (!ladder || !ladder->IsActive()) return true;
			if (ladder->mRow == row && ladder->mColumn == column) found = ladder.get();
			return false;
		}), mLadders.end());
	return found;
}

bool Board::RemoveLadderAt(int row, int column)
{
	bool removed = false;
	mLadders.erase(std::remove_if(mLadders.begin(), mLadders.end(),
		[&](const std::weak_ptr<Ladder>& weak) {
			auto ladder = weak.lock();
			if (!ladder || !ladder->IsActive()) return true;
			if (ladder->mRow != row || ladder->mColumn != column) return false;
			ladder->SetActive(false);
			GameObjectManager::GetInstance().DestroyGameObject(ladder.get());
			removed = true;
			return true;
		}), mLadders.end());
	return removed;
}

int Board::RemoveLaddersInRow(int row)
{
	int removed = 0;
	for (int column = 0; column < mColumns; ++column) {
		if (RemoveLadderAt(row, column)) ++removed;
	}
	return removed;
}

IceWall* Board::AddIceWall(int row, float centerX,
	int health, int maxHealth, float thawDamageRemainder,
	bool constructionComplete, int builderZombieID)
{
	if (row < 0 || row >= mRows || GetIceWall()
		|| (!constructionComplete && builderZombieID == NULL_ZOMBIE_ID)) return nullptr;
	auto wall = GameObjectManager::GetInstance().CreateGameObjectAsShared<IceWall>(
		LAYER_GAME_ZOMBIE, this, row, centerX,
		health, maxHealth, thawDamageRemainder,
		constructionComplete, builderZombieID);
	if (!wall) return nullptr;
	mIceWall = wall;
	return wall.get();
}

IceWall* Board::GetIceWall()
{
	auto wall = mIceWall.lock();
	if (!wall || !wall->IsActive()) {
		mIceWall.reset();
		return nullptr;
	}
	return wall.get();
}

IceWall* Board::GetIceWallInRow(int row)
{
	IceWall* wall = GetIceWall();
	return wall && wall->GetRow() == row ? wall : nullptr;
}

bool Board::RemoveIceWall(IceWall* wall)
{
	auto current = mIceWall.lock();
	if (!wall || !current || current.get() != wall) return false;
	mIceWall.reset();
	current->SetActive(false);
	GameObjectManager::GetInstance().DestroyGameObject(current);
	return true;
}

GroundRift* Board::AddGroundRift(int row, float frontX, int nextColumn,
	float downstreamDamageMultiplier)
{
	if (row < 0 || row >= mRows || mColumns <= 0 || !std::isfinite(frontX)
		|| !std::isfinite(downstreamDamageMultiplier)) return nullptr;
	auto rift = GameObjectManager::GetInstance().CreateGameObjectAsShared<GroundRift>(
		LAYER_GAME_PLANT, this, row, frontX,
		std::clamp(nextColumn, -1, mColumns - 1),
		std::clamp(downstreamDamageMultiplier, 0.0f, 1.0f));
	if (!rift) return nullptr;
	mGroundRifts.emplace_back(rift);
	return rift.get();
}

std::vector<GroundRift*> Board::GetGroundRifts()
{
	std::vector<GroundRift*> active;
	mGroundRifts.erase(std::remove_if(mGroundRifts.begin(), mGroundRifts.end(),
		[&active](const std::weak_ptr<GroundRift>& weak) {
			auto rift = weak.lock();
			if (!rift || !rift->IsActive()) return true;
			active.push_back(rift.get());
			return false;
		}), mGroundRifts.end());
	return active;
}

bool Board::RemoveGroundRift(GroundRift* rift)
{
	if (!rift) return false;
	for (auto it = mGroundRifts.begin(); it != mGroundRifts.end(); ++it) {
		auto current = it->lock();
		if (!current || !current->IsActive()) continue;
		if (current.get() != rift) continue;
		mGroundRifts.erase(it);
		current->SetActive(false);
		GameObjectManager::GetInstance().DestroyGameObject(current);
		return true;
	}
	return false;
}

int Board::RemoveLaddersInBlastSquare(
	const Vector& position, int centerRow, int cellRange)
{
	if (mRows <= 0 || mColumns <= 0) return 0;
	const int range = std::max(0, cellRange);
	const int blastRow = std::clamp(centerRow, 0, mRows - 1);
	// PixelToGridXKeepOnBoard 的当前场景等价换算：以格子左边界分段并钳在棋盘内。
	const int blastColumn = std::clamp(static_cast<int>(std::floor(
		(position.x - CELL_INITALIZE_POS_X) / CELL_COLLIDER_SIZE_X)),
		0, mColumns - 1);

	int removed = 0;
	mLadders.erase(std::remove_if(mLadders.begin(), mLadders.end(),
		[&](const std::weak_ptr<Ladder>& weak) {
			auto ladder = weak.lock();
			if (!ladder || !ladder->IsActive()) return true;
			if (std::abs(ladder->mRow - blastRow) > range
				|| std::abs(ladder->mColumn - blastColumn) > range) {
				return false;
			}
			ladder->SetActive(false);
			GameObjectManager::GetInstance().DestroyGameObject(ladder.get());
			++removed;
			return true;
		}), mLadders.end());
	return removed;
}

bool Board::ExtractNearestLadderForMagnet(
	int plantRow, int plantColumn, MagneticItem& item)
{
	Ladder* nearest = nullptr;
	float nearestScore = 0.0f;
	for (const auto& weak : mLadders) {
		auto ladder = weak.lock();
		if (!ladder || !ladder->IsActive()) continue;
		const int columnDistance = ladder->mColumn - plantColumn;
		const int rowDistance = ladder->mRow - plantRow;
		const int cellDistance = std::max(std::abs(columnDistance), std::abs(rowDistance));
		if (cellDistance > 2) continue;
		const float score = static_cast<float>(cellDistance)
			+ static_cast<float>(std::abs(rowDistance)) * 0.05f;
		if (!nearest || score < nearestScore) {
			nearest = ladder.get();
			nearestScore = score;
		}
	}
	if (!nearest) return false;

	item.textureKey = nearest->GetTextureKey();
	item.worldPosition = nearest->GetVisualCenter();
	item.destinationOffset = Vector(
		10.0f + GameRandom::Range(-10.0f, 10.0f),
		GameRandom::Range(-10.0f, 10.0f));
	item.drawScale = 0.8f;
	return RemoveLadderAt(nearest->mRow, nearest->mColumn);
}

Sun* Board::CreateSun(const Vector& position, bool needAnimation)
{
	auto sun = GameObjectManager::GetInstance().CreateGameObjectAsShared<Sun>
		(LAYER_GAME_COIN, this, position, 0.85f, "Sun",
			needAnimation, true);
	if (sun) {
		mEntityRegistry.AddCoin(sun);
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
		mEntityRegistry.AddCoin(sun);
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

/**
 * 从当前棋盘采集紧凑数值快照：实体提供真实生命/速度，卡槽提供未来种植候选，
 * 再把纯计算交给共享推演器。这里是唯一接触 GameObject 的边界。
 */
bool Board::BuildMonteCarloCombatSnapshot(
	PlantDefenseMonteCarlo::Snapshot& snapshot, bool mindControlledFaction,
	bool includeNightRoofChargeDetails)
{
	using namespace PlantDefenseMonteCarlo;
	if (mRows <= 0 || mColumns <= 0 || mColumns * mRows > 64) return false;
	snapshot = Snapshot{};
	snapshot.rows = mRows;
	snapshot.columns = mColumns;
	snapshot.sceneWidth = static_cast<float>(SCENE_WIDTH);
	snapshot.initialSun = static_cast<float>(std::max(0, mSun));
	snapshot.cells.reserve(static_cast<std::size_t>(mRows * mColumns));
	for (int row = 0; row < mRows; ++row) {
		for (int column = 0; column < mColumns; ++column) {
			const Cell* cell = GetCell(row, column);
			const Vector center = GetCellCenterPosition(row, column);
			snapshot.cells.push_back({
				row, column, center.x, center.y, cell && !cell->IsEmpty()
			});
		}
	}

	const auto& gameData = GameDataManager::GetInstance();
	const int backlineColumnCount = (mColumns + 1) / 2;
	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	snapshot.plants.reserve(plantIDs.size());
	snapshot.supports.reserve(std::min<std::size_t>(
		plantIDs.size(), static_cast<std::size_t>(mRows * mColumns)));
	for (const int plantID : plantIDs) {
		const Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (!plant || !plant->IsActive() || plant->IsSquished()
			|| plant->mRow < 0 || plant->mRow >= mRows
			|| plant->mColumn < 0 || plant->mColumn >= mColumns) {
			continue;
		}
		const PlantSimulationProfile& profile =
			gameData.GetPlantSimulationProfile(plant->mPlantType);
		const bool sleeping = plant->GetSleepState();
		float strategicValue = static_cast<float>(
			gameData.GetPlantSunCost(plant->mPlantType));
		if (profile.sunPerSecond > 0.0f) {
			strategicValue += kMonteCarloSunProducerFutureValue;
		}
		if (plant->mColumn < backlineColumnCount) {
			strategicValue *= kMonteCarloBacklineMultiplier;
		}

		const ColliderComponent* collider = plant->GetColliderComponent();
		const SDL_FRect bounds = collider
			? collider->GetBoundingBox()
			: SDL_FRect{
				plant->GetPosition().x - CELL_COLLIDER_SIZE_X * 0.5f,
				plant->GetPosition().y - CELL_COLLIDER_SIZE_Y * 0.5f,
				CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y
			};
		const Cell* cell = GetCell(plant->mRow, plant->mColumn);
		const bool isUnder = cell && cell->GetUnderPlantID() == plantID;
		const bool isNormal = cell && cell->GetNormalPlantID() == plantID;
		const bool isPumpkin = cell && cell->GetPumpkinPlantID() == plantID;
		const bool isOverlay = cell && cell->GetOverlayPlantID() == plantID;
		const bool executionLayer = isNormal || isPumpkin || isOverlay;
		if (profile.supportOnly) {
			// 普通花盆/睡莲只保留第二层阻挡所需数据，不占 128 株详细画像容量。
			snapshot.supports.push_back({
				plant->mPlantID,
				plant->mRow,
				plant->mColumn,
				plant->GetPosition().x,
				static_cast<float>(std::max(0, plant->mPlantHealth)),
				static_cast<float>(std::max(1, plant->mPlantMaxHealth)),
				strategicValue,
				{ bounds.x, bounds.y, bounds.w, bounds.h },
				true
			});
			continue;
		}
		bool protectedFromNightRoofCharge = false;
		if (includeNightRoofChargeDetails) {
			protectedFromNightRoofCharge =
				GetNightRoofChargeSupportProtector(plant) != nullptr;
			if (!protectedFromNightRoofCharge) {
				for (const int providerID : plantIDs) {
					const Plant* provider = mEntityRegistry.GetPlant(providerID);
					if (provider && provider->CanGroundNightRoofChargeFor(plant)) {
						protectedFromNightRoofCharge = true;
						break;
					}
				}
			}
		}
		snapshot.plants.push_back({
			plant->mPlantID,
			plant->mRow,
			plant->mColumn,
			plant->GetPosition().x,
			static_cast<float>(std::max(0, plant->mPlantHealth)),
			static_cast<float>(std::max(1, plant->mPlantMaxHealth)),
			strategicValue,
			sleeping ? 0.0f : profile.attackDps,
			profile.attackRowRadius,
			sleeping ? 0.0f : profile.sunPerSecond,
			0.0f,
			{ bounds.x, bounds.y, bounds.w, bounds.h },
			plant->mPlantType == PlantType::PLANT_PUMPKINSHELL,
			executionLayer ? plant->mRow * mColumns + plant->mColumn : -1,
			isNormal || isPumpkin,
			executionLayer,
			GetNightRoofHijackerSupportProtector(plant) != nullptr,
			isPumpkin ? 2 : (isNormal ? 1 : (isUnder ? 0 : -1)),
			plant->CanBeEaten() || isUnder,
			plant->GetShutdownTimeRemaining(),
			protectedFromNightRoofCharge,
			sleeping ? 0.0f : profile.slowApplicationsPerSecond,
			profile.slowDuration,
			sleeping ? 0.0f : profile.frozenApplicationsPerSecond,
			profile.frozenDuration,
			sleeping ? 0.0f : profile.butterApplicationsPerSecond,
			profile.butterDuration,
			sleeping ? 0.0f : profile.paralysisApplicationsPerSecond,
			profile.paralysisDuration
		});
		PlantDefenseMonteCarlo::PlantSnapshot& plantSnapshot =
			snapshot.plants.back();
		plantSnapshot.y = plant->GetPosition().y;
		plantSnapshot.abilityCooldownRemaining = sleeping
			? 0.0f : plant->GetSimulationAbilityCooldownRemaining();
		plantSnapshot.magneticPulseCooldown = sleeping
			? 0.0f : profile.magneticPulseCooldown;
		plantSnapshot.magneticPulseRadius = profile.magneticPulseRadius;
		plantSnapshot.magneticPulseParalysisDuration =
			profile.magneticPulseParalysisDuration;
		plantSnapshot.magneticSearchRowRadius = profile.magneticSearchRowRadius;
		plantSnapshot.magneticSearchRadius =
			profile.magneticSearchRadiusInCells * CELL_COLLIDER_SIZE_X;
		plantSnapshot.magneticEatingSearchRadius =
			profile.magneticEatingSearchRadiusInCells * CELL_COLLIDER_SIZE_X;
		plantSnapshot.magneticRowDistancePenalty = CELL_COLLIDER_SIZE_X;
		plantSnapshot.cobBlastCooldown = sleeping
			? 0.0f : profile.cobBlastCooldown;
		plantSnapshot.cobBlastDamage = sleeping
			? 0.0f : profile.cobBlastDamage;
		plantSnapshot.cobBlastRadius = profile.cobBlastRadius;
		plantSnapshot.cobBlastRowRadius = profile.cobBlastRowRadius;
		if (!sleeping) {
			if (const auto* cannon = dynamic_cast<const CobCannon*>(plant)) {
				const float pendingDelay = cannon->GetPendingSimulationBlastDelay();
				if (pendingDelay >= 0.0f) {
					const Vector target = cannon->GetPendingTarget();
					snapshot.pendingCobBlasts.push_back({
						plant->mPlantID, cannon->GetPendingTargetRow(),
						target.x, target.y, pendingDelay,
						profile.cobBlastDamage, profile.cobBlastRadius,
						profile.cobBlastRowRadius
					});
				}
			}
		}
	}

	// 已离膛玉米棒是独立提交效果；来源植物随后被移除也不能回滚这次固定落点。
	const PlantSimulationProfile& cobProfile =
		gameData.GetPlantSimulationProfile(PlantType::PLANT_COBCANNON);
	std::vector<int> bulletIDs = mEntityRegistry.GetAllBulletIDs();
	std::sort(bulletIDs.begin(), bulletIDs.end());
	for (const int bulletID : bulletIDs) {
		const Bullet* bullet = mEntityRegistry.GetBullet(bulletID);
		if (!bullet || !bullet->IsActive() || !bullet->IsCobCannonMotion()) continue;
		const Vector target = bullet->GetCobTarget();
		snapshot.pendingCobBlasts.push_back({
			-1, bullet->GetCobTargetRow(), target.x, target.y,
			std::max(0.0f, bullet->GetCobDuration() - bullet->GetCobElapsed()),
			static_cast<float>(std::max(0, bullet->GetBulletDamage())),
			cobProfile.cobBlastRadius,
			cobProfile.cobBlastRowRadius
		});
	}

	std::vector<int> zombieIDs = mEntityRegistry.GetAllZombieIDs();
	std::sort(zombieIDs.begin(), zombieIDs.end());
	snapshot.zombies.reserve(zombieIDs.size());
	for (const int zombieID : zombieIDs) {
		const Zombie* zombie = mEntityRegistry.GetZombie(zombieID);
		if (!zombie || !zombie->IsActive() || zombie->IsDying()
			|| !zombie->HasHead()
			|| (zombie->IsMindControlled() != mindControlledFaction
				&& !(includeNightRoofChargeDetails
					&& zombie->IsNightRoofChargeGuideType()))
			|| zombie->mRow < 0 || zombie->mRow >= mRows) {
			continue;
		}
		const bool simulatedCombatant =
			zombie->IsMindControlled() == mindControlledFaction;
		const auto* insulator = dynamic_cast<const InsulatorZombie*>(zombie);
		const bool canProtectNightRoofCharge = includeNightRoofChargeDetails
			&& insulator && !insulator->IsWet()
			&& zombie->mHelmHealth > 0
			&& zombie->CanBeAffectedByGroundHazards();
		auto getSimulatedImmunityRemaining = [zombie](ZombieControlEffect effect) {
			const float timed = zombie->GetControlImmunityTimeRemaining(effect);
			// 永久免疫没有有限计时；用最大 float 让纯数值时域自然保持门禁。
			return timed > 0.0f ? timed
				: (zombie->IsControlImmune(effect)
					? std::numeric_limits<float>::max() : 0.0f);
		};
		float centerX = zombie->GetPosition().x;
		float centerY = zombie->GetPosition().y;
		SDL_FRect zombieBounds{
			centerX - CELL_COLLIDER_SIZE_X * 0.5f,
			centerY - CELL_COLLIDER_SIZE_Y * 0.5f,
			CELL_COLLIDER_SIZE_X,
			CELL_COLLIDER_SIZE_Y
		};
		if (const ColliderComponent* collider = zombie->GetColliderComponent()) {
			zombieBounds = collider->GetBoundingBox();
			centerX = zombieBounds.x + zombieBounds.w * 0.5f;
			centerY = zombieBounds.y + zombieBounds.h * 0.5f;
		}
		snapshot.zombies.push_back({
			zombie->mZombieID,
			zombie->IsEating() ? zombie->GetEatingPlantID() : -1,
			zombie->mRow,
			centerX,
			centerY,
			zombie->GetUncontrolledHorizontalMoveSpeed(),
			static_cast<float>(std::max(0, zombie->mBodyHealth)),
			static_cast<float>(std::max(0, zombie->mBodyMaxHealth)),
			static_cast<float>(std::max(0, zombie->mHelmHealth)),
			static_cast<float>(std::max(0, zombie->mHelmMaxHealth)),
			static_cast<float>(std::max(0, zombie->mShieldHealth)),
			static_cast<float>(std::max(0, zombie->mShieldMaxHealth)),
			static_cast<float>(std::max(0, zombie->mAttackDamage)),
			zombie->IsEating(),
			zombie->GetCooldownTimer(),
			zombie->GetFrozenTimer(),
			zombie->GetButterTimer(),
			zombie->GetParalysisTimeRemaining(),
			getSimulatedImmunityRemaining(ZombieControlEffect::SLOW),
			getSimulatedImmunityRemaining(ZombieControlEffect::FROZEN),
			getSimulatedImmunityRemaining(ZombieControlEffect::BUTTER),
			getSimulatedImmunityRemaining(ZombieControlEffect::PARALYSIS),
			zombie->CanBeChilled(),
			zombie->CanBeFrozen(),
			zombie->CanBeButtered(),
			zombie->CanBeParalyzed(),
			zombie->CanBeAffectedByGroundHazards(),
			canProtectNightRoofCharge,
			includeNightRoofChargeDetails
				&& IsNightRoofChargeProtectionSuppressed(zombie),
			canProtectNightRoofCharge
				? 1.5f * CELL_COLLIDER_SIZE_X : 0.0f,
			zombie->IsMindControlled(),
			simulatedCombatant,
			false
		});
		PlantDefenseMonteCarlo::ZombieSnapshot& zombieSnapshot =
			snapshot.zombies.back();
		zombieSnapshot.bounds = {
			zombieBounds.x, zombieBounds.y, zombieBounds.w, zombieBounds.h
		};
		zombieSnapshot.magneticItemAvailable =
			zombie->CanBeTargetedByMagnetShroom();
		const MagneticSimulationLayer magneticLayer =
			zombie->GetMagneticSimulationLayer();
		zombieSnapshot.magneticRemovesHelm =
			magneticLayer == MagneticSimulationLayer::HELM;
		zombieSnapshot.magneticRemovesShield =
			magneticLayer == MagneticSimulationLayer::SHIELD;
	}

	if (mCardSlotManager) {
		const auto& cards = mCardSlotManager->GetCards();
		snapshot.cards.reserve(cards.size());
		for (Card* card : cards) {
			if (!card) continue;
			const PlantType type = card->GetPlantType();
			const PlantSimulationProfile& profile =
				gameData.GetPlantSimulationProfile(type);
			if (!profile.persistent || !profile.futurePlantable
				|| profile.supportOnly) continue;
			const bool dormant = profile.daytimeDormant
				&& !GameAPP::GetInstance().GetBackgroundIsNight(mBackGround);

			std::uint64_t legalCellMask = 0;
			for (int row = 0; row < mRows; ++row) {
				for (int column = 0; column < mColumns; ++column) {
					const int cellIndex = row * mColumns + column;
					if (CanPlantAt(type, row, column)) {
						legalCellMask |= (1ULL << cellIndex);
					}
				}
			}
			if (legalCellMask == 0) continue;

			const int cost = card->GetSunCost();
			float strategicValue = static_cast<float>(cost);
			if (!dormant && profile.sunPerSecond > 0.0f) {
				strategicValue += kMonteCarloSunProducerFutureValue;
			}
			snapshot.cards.push_back({
				static_cast<int>(type),
				cost,
				card->GetCooldownTimer(),
				card->GetCooldownTime(),
				static_cast<float>(profile.baseHealth),
				strategicValue,
				dormant ? 0.0f : profile.attackDps,
				profile.attackRowRadius,
				dormant ? 0.0f : profile.sunPerSecond,
				profile.firstSunDelay,
				legalCellMask,
				type == PlantType::PLANT_PUMPKINSHELL,
				type == PlantType::PLANT_PUMPKINSHELL ? 2
					: (IsUnderPlantLayerType(type) ? 0 : 1),
				dormant ? 0.0f : profile.slowApplicationsPerSecond,
				profile.slowDuration,
				dormant ? 0.0f : profile.frozenApplicationsPerSecond,
				profile.frozenDuration,
				dormant ? 0.0f : profile.butterApplicationsPerSecond,
				profile.butterDuration,
				dormant ? 0.0f : profile.paralysisApplicationsPerSecond,
				profile.paralysisDuration
			});
			PlantDefenseMonteCarlo::CardSnapshot& cardSnapshot =
				snapshot.cards.back();
			cardSnapshot.magneticPulseCooldown = dormant
				? 0.0f : profile.magneticPulseCooldown;
			cardSnapshot.magneticPulseRadius = profile.magneticPulseRadius;
			cardSnapshot.magneticPulseParalysisDuration =
				profile.magneticPulseParalysisDuration;
			cardSnapshot.magneticSearchRowRadius = profile.magneticSearchRowRadius;
			cardSnapshot.magneticSearchRadius =
				profile.magneticSearchRadiusInCells * CELL_COLLIDER_SIZE_X;
			cardSnapshot.magneticEatingSearchRadius =
				profile.magneticEatingSearchRadiusInCells * CELL_COLLIDER_SIZE_X;
			cardSnapshot.magneticRowDistancePenalty = CELL_COLLIDER_SIZE_X;
			cardSnapshot.cobBlastCooldown = dormant
				? 0.0f : profile.cobBlastCooldown;
			cardSnapshot.cobBlastDamage = dormant
				? 0.0f : profile.cobBlastDamage;
			cardSnapshot.cobBlastRadius = profile.cobBlastRadius;
			cardSnapshot.cobBlastRowRadius = profile.cobBlastRowRadius;
		}
	}
	return true;
}

void Board::CreateCobCannonExplosion(const Vector& position, int targetRow, int damage)
{
	constexpr float kCobBlastRadius = 115.0f; // 原版 CobBig 爆心半径，单位：px
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("CobCannonBlastMark", position);
		g_particleSystem->EmitEffect("CobCannonPopcorn", position);
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_DOOMSHROOM, 0.5f);
	ShakeBoard(3.0f, -4.0f);

	for (int row = targetRow - 1; row <= targetRow + 1; ++row) {
		mEntityRegistry.ForEachZombieInRow(row, [&](Zombie* zombie) {
			if (!zombie || !zombie->IsActive() || zombie->IsDying()
				|| zombie->IsMindControlled()
				|| !zombie->CanBeAffectedByCobCannonExplosion()) return;
			SDL_FRect bounds{};
			if (const ColliderComponent* collider = zombie->GetColliderComponent()) {
				bounds = collider->GetBoundingBox();
			}
			else {
				const Vector zombiePosition = zombie->GetPosition();
				bounds = { zombiePosition.x - 25.0f, zombiePosition.y - 65.0f,
					50.0f, 100.0f };
			}
			const float nearestX = std::clamp(position.x, bounds.x, bounds.x + bounds.w);
			const float nearestY = std::clamp(position.y, bounds.y, bounds.y + bounds.h);
			const float dx = position.x - nearestX;
			const float dy = position.y - nearestY;
			if (dx * dx + dy * dy <= kCobBlastRadius * kCobBlastRadius) {
				zombie->TakePlantAshDamage(damage);
			}
		});
	}
	// 原版玉米炮与樱桃炸弹相同：扶梯按爆心格周围 3x3 方形范围清除。
	RemoveLaddersInBlastSquare(position, targetRow, 1);
}

bool Board::PickMonteCarloPlantBlastTarget(
	int minRow, int maxRow, int damage, float radius, int sourceZombieID,
	int& targetRow, Vector& targetPosition, MonteCarloTargetStats* stats,
	const std::vector<int>* removalPlantIDs, int* selectedRemovalPlantID,
	float removalStrikeInterval, int removalStrikeDamage,
	const std::vector<int>* removalStrikeCounts)
{
	using namespace PlantDefenseMonteCarlo;
	const bool removalMode = removalPlantIDs != nullptr;
	if (removalMode ? removalPlantIDs->empty()
		: (damage <= 0 || radius <= 0.0f)) {
		return false;
	}
	minRow = std::clamp(minRow, 0, std::max(0, mRows - 1));
	maxRow = std::clamp(maxRow, minRow, std::max(0, mRows - 1));

	Snapshot snapshot;
	if (!BuildMonteCarloCombatSnapshot(snapshot, false)) return false;
	std::vector<std::pair<int, int>> candidateCells;
	const std::unordered_set<int> eligibleRemovalIDs = removalMode
		? std::unordered_set<int>(removalPlantIDs->begin(), removalPlantIDs->end())
		: std::unordered_set<int>();
	std::unordered_map<int, int> strikeCountByPlantID;
	if (removalMode && removalStrikeCounts
		&& removalStrikeCounts->size() == removalPlantIDs->size()) {
		for (std::size_t i = 0; i < removalPlantIDs->size(); ++i) {
			strikeCountByPlantID.emplace(
				(*removalPlantIDs)[i], (*removalStrikeCounts)[i]);
		}
	}
	for (const PlantSnapshot& plant : snapshot.plants) {
		if (removalMode && eligibleRemovalIDs.find(plant.id) != eligibleRemovalIDs.end()) {
			const Vector center = GetCellCenterPosition(plant.row, plant.column);
			snapshot.candidates.push_back({
				plant.row, plant.column, center.x, center.y, plant.id
			});
			Candidate& candidate = snapshot.candidates.back();
			const auto strikeCount = strikeCountByPlantID.find(plant.id);
			if (strikeCount != strikeCountByPlantID.end()) {
				candidate.targetStrikeInterval = removalStrikeInterval;
				candidate.targetStrikeDamage = static_cast<float>(removalStrikeDamage);
				candidate.targetStrikeCount = std::max(0, strikeCount->second);
			}
		}
		else if (!removalMode && plant.row >= minRow && plant.row <= maxRow) {
			candidateCells.emplace_back(plant.row, plant.column);
		}
	}
	for (const SupportSnapshot& support : snapshot.supports) {
		if (removalMode
			&& eligibleRemovalIDs.find(support.id) != eligibleRemovalIDs.end()) {
			const Vector center = GetCellCenterPosition(support.row, support.column);
			snapshot.candidates.push_back({
				support.row, support.column, center.x, center.y, support.id
			});
		}
		else if (!removalMode
			&& support.row >= minRow && support.row <= maxRow) {
			candidateCells.emplace_back(support.row, support.column);
		}
	}
	if (!removalMode) {
		std::sort(candidateCells.begin(), candidateCells.end());
		candidateCells.erase(
			std::unique(candidateCells.begin(), candidateCells.end()),
			candidateCells.end());
		for (const auto& cell : candidateCells) {
			const Vector center = GetCellCenterPosition(cell.first, cell.second);
			snapshot.candidates.push_back({
				cell.first, cell.second, center.x, center.y
			});
		}
	}
	if (snapshot.candidates.empty()) return false;

	Config config;
	config.rolloutCount = kPlantTargetMonteCarloRolloutCount;
	config.maxZombiesPerRollout = kMonteCarloMaxZombies;
	config.horizonSeconds = kMonteCarloHorizonSeconds;
	config.stepSeconds = kMonteCarloStepSeconds;
	config.impactDamage = static_cast<float>(damage);
	config.impactRadius = radius;
	config.pumpkinProtectionCellRadius = kPumpkinProtectionCellRadius;
	config.pumpkinImpactDamageMultiplier =
		static_cast<float>(kPumpkinAreaDamageMultiplier);
	config.plantDecisionInterval = kMonteCarloPlantDecisionSeconds;
	config.terminalBlockedSecondUtility =
		kMonteCarloTerminalBlockedSecondUtility;
	config.terminalBlockedSecondsCap =
		kMonteCarloTerminalBlockedSecondsCap;

	std::uint32_t seed = 2166136261u;
	auto mixSeed = [&seed](std::uint32_t value) {
		seed ^= value;
		seed *= 16777619u;
	};
	mixSeed(static_cast<std::uint32_t>(mBoardFrame));
	mixSeed(static_cast<std::uint32_t>(mCurrentWave));
	mixSeed(static_cast<std::uint32_t>(sourceZombieID));
	const Result result = ChooseTarget(snapshot, config, seed);
	if (stats) {
		stats->rolloutCount = result.rolloutCount;
		stats->candidateCount =
			static_cast<int>(snapshot.candidates.size());
		stats->sampledZombieCount = result.sampledZombieCount;
		stats->sampledPlantCount = result.sampledPlantCount;
		stats->supportPlantCount = result.supportPlantCount;
		stats->cardCount = result.cardCount;
		stats->bestScore = result.score;
		stats->coordinationLoss = result.coordinationLoss;
	}
	if (result.candidateIndex < 0
		|| result.candidateIndex >= static_cast<int>(snapshot.candidates.size())) {
		return false;
	}
	const Candidate& chosen = snapshot.candidates[result.candidateIndex];
	targetRow = chosen.row;
	targetPosition = Vector(chosen.x, chosen.y);
	if (selectedRemovalPlantID) {
		*selectedRemovalPlantID = chosen.targetPlantId;
	}
	return true;
}

bool Board::PickMonteCarloPlantRemovalTarget(
	const std::vector<int>& eligiblePlantIDs, int sourceZombieID,
	int& targetPlantID, MonteCarloTargetStats* stats,
	float strikeInterval, int strikeDamage,
	const std::vector<int>* strikeCounts)
{
	int targetRow = -1;
	Vector targetPosition;
	targetPlantID = NULL_PLANT_ID;
	return PickMonteCarloPlantBlastTarget(
		0, std::max(0, mRows - 1), 0, 0.0f, sourceZombieID,
		targetRow, targetPosition, stats, &eligiblePlantIDs, &targetPlantID,
		strikeInterval, strikeDamage, strikeCounts);
}

bool Board::TryClaimMonteCarloHealerDecisionSlot()
{
	if (mMonteCarloHealerDecisionCooldownSteps > 0) return false;
	mMonteCarloHealerDecisionCooldownSteps =
		kMonteCarloHealerDecisionSpacingSteps;
	return true;
}

bool Board::PickMonteCarloZombieTreatment(
	const MonteCarloTreatmentRequest& request,
	MonteCarloTreatmentDecision& decision, MonteCarloTargetStats* stats)
{
	PROFILE_SCOPE("MC.Healer.Total");
	using namespace PlantDefenseMonteCarlo;
	Zombie* source = mEntityRegistry.GetZombie(request.sourceZombieID);
	if (!source || source->IsMindControlled() || !source->IsActive()
		|| source->IsDying() || !source->HasHead()
		|| request.castSeconds <= 0.0f
		|| request.areaRadius <= 0.0f || request.focusedRadius <= 0.0f
		|| request.areaHealAmount <= 0.0f || request.focusedHealAmount <= 0.0f) {
		return false;
	}

	Snapshot snapshot;
	bool snapshotBuilt = false;
	{
		PROFILE_SCOPE("MC.Healer.Snapshot");
		snapshotBuilt = BuildMonteCarloCombatSnapshot(snapshot, false);
	}
	if (!snapshotBuilt) return false;
	std::unordered_set<int> areaTargetIDs(
		request.areaTargetIDs.begin(), request.areaTargetIDs.end());
	std::unordered_set<int> focusedTargetIDs(
		request.focusedTargetIDs.begin(), request.focusedTargetIDs.end());
	std::unordered_set<int> forcedZombieIDs{ request.sourceZombieID };
	const int lockedHijackerID = GetNightRoofHijackerID();
	if (lockedHijackerID != NULL_ZOMBIE_ID) forcedZombieIDs.insert(lockedHijackerID);

	std::vector<PendingTreatment> pendingTreatments;
	for (const int zombieID : mEntityRegistry.GetAllZombieIDs()) {
		auto* healer = dynamic_cast<HealerZombie*>(mEntityRegistry.GetZombie(zombieID));
		if (!healer || healer->mZombieID == request.sourceZombieID
			|| !healer->IsActive() || healer->IsDying()
			|| healer->IsMindControlled() != source->IsMindControlled()) {
			continue;
		}
		const HealerZombie::TreatmentState state = healer->GetTreatmentState();
		if (state != HealerZombie::TreatmentState::AREA
			&& state != HealerZombie::TreatmentState::FOCUSED) {
			continue;
		}
		forcedZombieIDs.insert(healer->mZombieID);
		if (state == HealerZombie::TreatmentState::FOCUSED) {
			forcedZombieIDs.insert(healer->GetFocusedTargetID());
		}
		pendingTreatments.push_back({
			state == HealerZombie::TreatmentState::AREA
				? TreatmentAction::AREA : TreatmentAction::FOCUSED,
			healer->mZombieID,
			healer->GetFocusedTargetID(),
			healer->GetCastRemaining(),
			state == HealerZombie::TreatmentState::AREA
				? request.areaRadius : request.focusedRadius,
			state == HealerZombie::TreatmentState::AREA
				? request.areaHealAmount : request.focusedHealAmount
		});
	}

	struct RankedZombie {
		ZombieSnapshot snapshot;
		float priority = 0.0f;
		bool forced = false;
	};
	std::vector<RankedZombie> ranked;
	ranked.reserve(snapshot.zombies.size());
	for (const ZombieSnapshot& zombie : snapshot.zombies) {
		const float health = zombie.bodyHealth
			+ zombie.helmHealth + zombie.shieldHealth;
		const float distanceFactor = 1.0f
			+ 400.0f / std::max(100.0f, zombie.x);
		float priority = std::max(1.0f, health)
			* std::max(1.0f, zombie.attackDamage)
			* std::max(1.0f, zombie.moveSpeed) * distanceFactor;
		if (zombie.id == request.sourceZombieID) {
			priority = std::numeric_limits<float>::max();
		}
		else if (zombie.id == lockedHijackerID) {
			priority = std::numeric_limits<float>::max() * 0.5f;
		}
		else if (focusedTargetIDs.find(zombie.id) != focusedTargetIDs.end()) {
			priority *= 2.0f;
		}
		else if (areaTargetIDs.find(zombie.id) != areaTargetIDs.end()) {
			priority *= 1.5f;
		}
		ranked.push_back({
			zombie, priority,
			forcedZombieIDs.find(zombie.id) != forcedZombieIDs.end()
		});
	}
	std::sort(ranked.begin(), ranked.end(),
		[](const RankedZombie& lhs, const RankedZombie& rhs) {
			if (lhs.forced != rhs.forced) return lhs.forced;
			if (lhs.priority != rhs.priority) return lhs.priority > rhs.priority;
			return lhs.snapshot.id < rhs.snapshot.id;
		});
	if (ranked.size() > static_cast<std::size_t>(kMonteCarloMaxZombies)) {
		ranked.resize(kMonteCarloMaxZombies);
	}
	snapshot.zombies.clear();
	std::unordered_set<int> sampledZombieIDs;
	for (const RankedZombie& zombie : ranked) {
		snapshot.zombies.push_back(zombie.snapshot);
		sampledZombieIDs.insert(zombie.snapshot.id);
	}
	if (sampledZombieIDs.find(request.sourceZombieID) == sampledZombieIDs.end()) {
		return false;
	}

	float areaOverflowPressure = 0.0f;
	for (const int zombieID : request.areaTargetIDs) {
		if (sampledZombieIDs.find(zombieID) != sampledZombieIDs.end()) continue;
		const Zombie* zombie = mEntityRegistry.GetZombie(zombieID);
		if (!zombie || !zombie->IsActive() || zombie->IsDying()) continue;
		auto repairPotential = [&request](int current, int maximum) {
			if (current <= 0 || maximum <= 0 || current >= maximum) return 0.0f;
			return std::min(request.areaHealAmount,
				static_cast<float>(maximum - current));
		};
		const float restored = repairPotential(
			zombie->mBodyHealth, zombie->mBodyMaxHealth)
			+ repairPotential(zombie->mHelmHealth, zombie->mHelmMaxHealth)
			+ repairPotential(zombie->mShieldHealth, zombie->mShieldMaxHealth);
		const float attackFactor = 0.5f
			+ static_cast<float>(std::max(0, zombie->mAttackDamage)) / 50.0f;
		const float progressFactor = 1.0f + std::max(
			0.0f, 900.0f - zombie->GetPosition().x) / 900.0f;
		areaOverflowPressure += restored * attackFactor * progressFactor
			* kTreatmentTerminalPressurePerHealth;
	}

	std::vector<TreatmentCandidate> candidates;
	if (!request.areaTargetIDs.empty()) {
		candidates.push_back({
			TreatmentAction::AREA, NULL_ZOMBIE_ID, 0.0f, areaOverflowPressure
		});
	}
	for (const int targetID : request.focusedTargetIDs) {
		if (sampledZombieIDs.find(targetID) == sampledZombieIDs.end()) continue;
		candidates.push_back({ TreatmentAction::FOCUSED, targetID, 0.0f, 0.0f });
	}
	if (candidates.empty()) return false;
	if (request.allowWait && request.waitSeconds > 0.0f) {
		const std::size_t immediateCount = candidates.size();
		candidates.reserve(immediateCount * 2);
		for (std::size_t i = 0; i < immediateCount; ++i) {
			TreatmentCandidate delayed = candidates[i];
			delayed.delaySeconds = request.waitSeconds;
			candidates.push_back(delayed);
		}
	}

	TreatmentConfig config;
	config.combat.rolloutCount = kTreatmentMonteCarloRolloutCount;
	config.combat.maxZombiesPerRollout = kMonteCarloMaxZombies;
	config.combat.horizonSeconds = kTreatmentMonteCarloHorizonSeconds;
	config.combat.stepSeconds = kMonteCarloStepSeconds;
	config.combat.plantDecisionInterval = kMonteCarloPlantDecisionSeconds;
	config.combat.terminalBlockedSecondUtility =
		kMonteCarloTerminalBlockedSecondUtility;
	config.combat.terminalBlockedSecondsCap =
		kMonteCarloTerminalBlockedSecondsCap;
	config.sourceZombieId = request.sourceZombieID;
	config.castSeconds = request.castSeconds;
	config.areaRadius = request.areaRadius;
	config.focusedRadius = request.focusedRadius;
	config.areaHealAmount = request.areaHealAmount;
	config.focusedHealAmount = request.focusedHealAmount;
	config.terminalZombiePressurePerHealth = kTreatmentTerminalPressurePerHealth;
	config.hijackerZombieId = lockedHijackerID;
	config.survivalMode = mIsSurvival;
	config.survivalExecutionLineCap =
		static_cast<float>(kNightRoofHijackerSurvivalLineCap);
	if (lockedHijackerID != NULL_ZOMBIE_ID) {
		if (mNightRoofChargePhase == NightRoofChargePhase::WARNING) {
			config.hijackerExecutionSeconds = mNightRoofChargePhaseTimer;
		}
		else if (mNightRoofChargePhase == NightRoofChargePhase::CHARGING) {
			float chargePerSecond = 0.0f;
			switch (mRainIntensity) {
			case RainIntensity::LIGHT: chargePerSecond = kNightRoofChargeLightPerSecond; break;
			case RainIntensity::MEDIUM: chargePerSecond = kNightRoofChargeMediumPerSecond; break;
			case RainIntensity::HEAVY: chargePerSecond = kNightRoofChargeHeavyPerSecond; break;
			default: break;
			}
			chargePerSecond += GetNightRoofHijackerRainChargeBonusPerSecond();
			if (chargePerSecond > 0.0f) {
				config.hijackerExecutionSeconds = std::max(
					0.0f, (kNightRoofChargeMaximum - mNightRoofCharge)
						/ chargePerSecond) + kNightRoofHijackerWarningDuration;
			}
		}
	}

	std::uint32_t seed = 2166136261u;
	auto mixSeed = [&seed](std::uint32_t value) {
		seed ^= value;
		seed *= 16777619u;
	};
	mixSeed(static_cast<std::uint32_t>(mBoardFrame));
	mixSeed(static_cast<std::uint32_t>(mCurrentWave));
	mixSeed(static_cast<std::uint32_t>(request.sourceZombieID));
	TreatmentResult result;
	{
		PROFILE_SCOPE("MC.Healer.Rollouts");
		result = ChooseTreatment(
			snapshot, candidates, pendingTreatments, config, seed);
	}
	if (stats) {
		stats->rolloutCount = result.rolloutCount;
		stats->candidateCount = static_cast<int>(candidates.size());
		stats->sampledZombieCount = result.sampledZombieCount;
		stats->sampledPlantCount = result.sampledPlantCount;
		stats->supportPlantCount = result.supportPlantCount;
		stats->cardCount = result.cardCount;
		stats->bestScore = result.score;
		stats->coordinationLoss = 0.0f;
	}
	if (result.candidateIndex < 0
		|| result.candidateIndex >= static_cast<int>(candidates.size())) {
		return false;
	}
	const TreatmentCandidate& chosen = candidates[result.candidateIndex];
	if (chosen.delaySeconds > 0.0f) {
		decision.action = MonteCarloTreatmentAction::WAIT;
		decision.targetZombieID = NULL_ZOMBIE_ID;
	}
	else if (chosen.action == TreatmentAction::AREA) {
		decision.action = MonteCarloTreatmentAction::AREA;
		decision.targetZombieID = NULL_ZOMBIE_ID;
	}
	else {
		decision.action = MonteCarloTreatmentAction::FOCUSED;
		decision.targetZombieID = chosen.targetZombieId;
	}
	return true;
}

bool Board::IsValidCobCannonAnchor(int row, int anchorColumn) const
{
	const PlantFootprint footprint = GetPlantFootprint(PlantType::PLANT_COBCANNON);
	for (std::size_t i = 0; i < footprint.count; ++i) {
		const int occupiedRow = row + footprint.cells[i].rowOffset;
		const int occupiedColumn = anchorColumn + footprint.cells[i].columnOffset;
		if (occupiedRow < 0 || occupiedRow >= mRows
			|| occupiedColumn < 0 || occupiedColumn >= mColumns) {
			return false;
		}
		Plant* kernel = GetNormalPlantAt(occupiedRow, occupiedColumn);
		if (!kernel || !kernel->IsActive() || kernel->mPlantHealth <= 0
			|| kernel->IsSquished() || kernel->IsBungeeTargeted()
			|| kernel->mPlantType != PlantType::PLANT_KERNELPULT
			|| GetPumpkinAt(occupiedRow, occupiedColumn)
			|| GetOverlayPlantAt(occupiedRow, occupiedColumn)) {
			return false;
		}
	}
	return true;
}

bool Board::ResolvePlantPlacementAnchor(PlantType type, int row, int col,
	int& anchorRow, int& anchorColumn) const
{
	anchorRow = row;
	anchorColumn = col;
	if (type != PlantType::PLANT_COBCANNON) {
		return row >= 0 && row < mRows && col >= 0 && col < mColumns;
	}
	if (IsValidCobCannonAnchor(row, col)) return true;
	if (IsValidCobCannonAnchor(row, col - 1)) {
		anchorColumn = col - 1;
		return true;
	}
	return false;
}

bool Board::OccupyPlantFootprint(PlantType type, int row, int anchorColumn,
	int plantID, const std::vector<int>& replacePlantIDs)
{
	const PlantFootprint footprint = GetPlantFootprint(type);
	for (std::size_t i = 0; i < footprint.count; ++i) {
		Cell* cell = GetCell(row + footprint.cells[i].rowOffset,
			anchorColumn + footprint.cells[i].columnOffset);
		if (!cell) return false;
		const int occupiedID = cell->GetNormalPlantID();
		if (occupiedID != NULL_PLANT_ID
			&& std::find(replacePlantIDs.begin(), replacePlantIDs.end(), occupiedID)
				== replacePlantIDs.end()) {
			return false;
		}
	}
	// 校验全部成功后才写入，避免半株植物残留在棋盘上。
	for (std::size_t i = 0; i < footprint.count; ++i) {
		GetCell(row + footprint.cells[i].rowOffset,
			anchorColumn + footprint.cells[i].columnOffset)->SetNormalPlantID(plantID);
	}
	return true;
}

bool Board::CanPlantAt(PlantType type, int row, int col)
{
	if (!HasPlantingQuota(type)) return false;
	int anchorRow = row;
	int anchorColumn = col;
	if (!ResolvePlantPlacementAnchor(type, row, col, anchorRow, anchorColumn)) return false;
	const PlantFootprint polarFootprint = GetPlantFootprint(type);
	for (std::size_t i = 0; i < polarFootprint.count; ++i) {
		if (HasSnowHoleAt(anchorRow + polarFootprint.cells[i].rowOffset,
			anchorColumn + polarFootprint.cells[i].columnOffset)) return false;
	}
	if (IsPlantFootprintFrozen(type, anchorRow, anchorColumn)) return false;
	if (IsIceAt(anchorRow, anchorColumn)) return false;
	if (type == PlantType::PLANT_COBCANNON) {
		const PlantFootprint footprint = GetPlantFootprint(type);
		for (std::size_t i = 0; i < footprint.count; ++i) {
			const int occupiedRow = anchorRow + footprint.cells[i].rowOffset;
			const int occupiedColumn = anchorColumn + footprint.cells[i].columnOffset;
			if (IsIceAt(occupiedRow, occupiedColumn)
				|| HasCraterAt(occupiedRow, occupiedColumn)) return false;
		}
		return IsValidCobCannonAnchor(anchorRow, anchorColumn);
	}

	row = anchorRow;
	col = anchorColumn;
	Cell* cell = GetCell(row, col);
	if (!cell || HasCraterAt(row, col)) return false;

	const bool isWater = IsPoolSquare(row, col);
	Plant* underPlant = mEntityRegistry.GetPlant(cell->GetUnderPlantID());
	Plant* normalPlant = mEntityRegistry.GetPlant(cell->GetNormalPlantID());
	const bool hasLilyPad = underPlant
		&& underPlant->mPlantType == PlantType::PLANT_LILYPAD;
	const bool hasFlowerPot = underPlant && underPlant->IsRoofSupportPlant();
	if (type == PlantType::PLANT_INSTANT_COFFEE) {
		// 原版 flying layer：只允许覆盖仍睡眠、尚未进入唤醒且未被蹦极抓取的普通层蘑菇。
		return cell->GetOverlayPlantID() == NULL_PLANT_ID
			&& normalPlant && normalPlant->GetSleepState()
			&& !normalPlant->IsWakingUp()
			&& !normalPlant->IsBungeeTargeted();
	}
	if (IsUpgradePlantType(type)) {
		// 紫卡按规则选择 normal 或 under；承载层升级不会检查或覆盖上层植物。
		Plant* basePlant = GetUpgradePlantLayer(type) == PlantUpgradeLayer::UNDER
			? underPlant : normalPlant;
		return basePlant && basePlant->IsActive()
			&& basePlant->mPlantHealth > 0 && !basePlant->IsSquished()
			&& !basePlant->IsBungeeTargeted()
			&& basePlant->mPlantType == GetUpgradeBasePlantType(type);
	}
	if (type == PlantType::PLANT_PUMPKINSHELL) {
		// 南瓜有独立外壳层，但水路与屋顶仍分别要求正确的承载植物。
		if (cell->GetPumpkinPlantID() != NULL_PLANT_ID) return false;
		if (normalPlant && IsMultiCellPlantType(normalPlant->mPlantType)) return false;
		if (isWater) return hasLilyPad;
		if (IsRoofBackground()) return hasFlowerPot;
		return true;
	}
	if (type == PlantType::PLANT_FLOWERPOT) {
		// 原版允许花盆落在任意非水地面；屋顶以外通常只是不推荐选择。
		return !isWater && cell->IsEmpty();
	}
	if ((type == PlantType::PLANT_SPIKEWEED
		|| type == PlantType::PLANT_SPIKEROCK)
		&& (isWater || IsSpikeweedTerrainRestricted(mBackGround))) {
		// 地刺系既不能隔着睡莲扎水面，也不能隔着花盆扎屋顶瓦片。
		return false;
	}
	if (IsRoofBackground()
		&& type != PlantType::PLANT_LILYPAD
		&& type != PlantType::PLANT_TANGLEKELP
		&& type != PlantType::PLANT_SEASHROOM) {
		// 屋顶普通植物只占 normal 层，必须由 under 层的花盆承载。
		return hasFlowerPot && cell->GetNormalPlantID() == NULL_PLANT_ID;
	}
	if (type == PlantType::PLANT_LILYPAD
		|| type == PlantType::PLANT_TANGLEKELP
		|| type == PlantType::PLANT_SEASHROOM) {
		// 三种水生植物都直接落水；后两者占普通层，因此空格判断也禁止叠在睡莲上。
		return isWater && cell->IsEmpty();
	}
	if (isWater) {
		// 土豆雷没有水面形态；即使已有睡莲也不能落在水路。
		if (type == PlantType::PLANT_POTATOMINE) return false;
		return hasLilyPad
			&& cell->GetNormalPlantID() == NULL_PLANT_ID;
	}
	return cell->GetNormalPlantID() == NULL_PLANT_ID
		&& (cell->GetUnderPlantID() == NULL_PLANT_ID || hasFlowerPot);
}

bool Board::HasPlantingQuota(PlantType type) const
{
	if (type == PlantType::PLANT_PLANTERN) {
		// 模仿者占位虽然还不是 Plantern 实例，也必须预留唯一名额。
		return mActivePlanternID == NULL_PLANT_ID;
	}
	return type != PlantType::PLANT_ELITE_SCAREDYSHROOM
		|| mEliteScaredyShroomsPlanted < kEliteScaredyShroomPlantLimit;
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
bool Board::ApplyPolarLobbedWind(int sourceRow, int& landingRow, Vector& target) const
{
	if (!SupportsPolarNightEnvironment() || !IsPolarWindDangerous()
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
			: (AdventureProgression::GetLevelNumberInArea(mLevel) == 9
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

bool Board::BeginCobCannonTargeting(int row, int col)
{
	if (!CanBeginCobCannonTargeting(row, col)) return false;
	auto* cannon = dynamic_cast<CobCannon*>(GetNormalPlantAt(row, col));
	mTargetingCobCannonID = cannon->mPlantID;
	mCursorObjectManager.Activate(CursorObjectType::COB_CANNON_TARGET, [this]() {
		mTargetingCobCannonID = NULL_PLANT_ID;
	});
	return true;
}

bool Board::CanBeginCobCannonTargeting(int row, int col) const
{
	if (mCursorObjectManager.GetActiveType() != CursorObjectType::NONE) return false;
	const auto* cannon = dynamic_cast<const CobCannon*>(GetNormalPlantAt(row, col));
	return cannon && cannon->IsActive() && cannon->IsReady();
}

bool Board::FireTargetedCobCannonAt(const Vector& target)
{
	if (!IsCobCannonTargeting()) return false;
	const float rowHeight = GetCellHeight();
	const float boardTop = mRows > 0
		? GetRowCenterYAtX(0, target.x) - rowHeight * 0.5f : -1.0f;
	if (mRows <= 0 || rowHeight <= 0.0f || boardTop < 0.0f || target.y < boardTop) {
		// 与原版草坪上界门禁一致：点到卡槽/UI 区只取消准星，不提交炮击。
		mCursorObjectManager.ClearActive();
		return false;
	}
	// 只用屋顶在 target.x 处的连续坡面划分逻辑行；target 本身保持玩家点击像素不变，
	// 避免原版先转格再写回 Y 所产生的屋顶“上界之风”落点偏移。
	const int targetRow = std::clamp(static_cast<int>(std::floor(
		(target.y - boardTop) / rowHeight)), 0, mRows - 1);
	auto* cannon = dynamic_cast<CobCannon*>(
		mEntityRegistry.GetPlant(mTargetingCobCannonID));
	const bool fired = cannon && cannon->IsActive()
		&& cannon->FireAt(target, targetRow);
	mCursorObjectManager.ClearActive();
	return fired;
}

void Board::CancelCobCannonTargeting(int plantID)
{
	if (plantID == NULL_PLANT_ID || plantID != mTargetingCobCannonID) return;
	mCursorObjectManager.ClearActive();
}

bool Board::HasPlantingRequirement(PlantType type) const
{
	if (type == PlantType::PLANT_COBCANNON) {
		for (int row = 0; row < mRows; ++row) {
			for (int column = 0; column < mColumns - 1; ++column) {
				if (IsValidCobCannonAnchor(row, column)) return true;
			}
		}
		return false;
	}
	const PlantType baseType = GetUpgradeBasePlantType(type);
	if (baseType == PlantType::NUM_PLANT_TYPES) return true;
	for (const int plantID : mEntityRegistry.GetAllPlantIDs()) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (plant && plant->IsActive() && !plant->IsSquished()
			&& plant->mPlantHealth > 0 && plant->mPlantType == baseType) {
			return true;
		}
	}
	return false;
}

int Board::GetEliteScaredyShroomPlantLimit() const
{
	return kEliteScaredyShroomPlantLimit;
}

Plant* Board::GetTopPlantAt(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) return nullptr;
	Cell* cell = mCells[row][col];
	return cell ? mEntityRegistry.GetPlant(cell->GetTopPlantID()) : nullptr;
}

Plant* Board::GetCatapultTargetPlantAt(int row, int col) const
{
	const std::array<Plant*, 4> layers = {
		GetOverlayPlantAt(row, col),
		GetNormalPlantAt(row, col),
		GetPumpkinAt(row, col),
		GetUnderPlantAt(row, col),
	};
	for (Plant* plant : layers) {
		if (plant && plant->IsActive() && plant->mPlantHealth > 0
			&& !plant->IsSquished() && plant->CanBeEaten()) {
			return plant;
		}
	}
	return nullptr;
}

Plant* Board::FindAirborneThreatProtector(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) return nullptr;

	// 原版按植物容器顺序返回第一株；实体 ID 保留种植先后，排序后可在重叠保护区稳定复刻。
	std::vector<int> plantIDs = mEntityRegistry.GetAllPlantIDs();
	std::sort(plantIDs.begin(), plantIDs.end());
	for (const int plantID : plantIDs) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (plant && plant->ProtectsCellFromAirborneThreat(row, col)) return plant;
	}
	return nullptr;
}

Plant* Board::GetJumpBlockingPlantAt(int row, int col, ZombieJumpType jumpType) const
{
	// 跳跃阻拦是格内植物能力，不等同于啃食顶层；南瓜等非阻拦外壳要继续向内查询。
	const std::array<Plant*, 3> layers = {
		GetPumpkinAt(row, col),
		GetNormalPlantAt(row, col),
		GetUnderPlantAt(row, col),
	};
	for (Plant* plant : layers) {
		if (plant && plant->IsActive() && plant->mPlantHealth > 0
			&& plant->BlocksZombieJump(jumpType)) {
			return plant;
		}
	}
	return nullptr;
}

Plant* Board::GetUnderPlantAt(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) return nullptr;
	Cell* cell = mCells[row][col];
	return cell ? mEntityRegistry.GetPlant(cell->GetUnderPlantID()) : nullptr;
}

Plant* Board::GetNormalPlantAt(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) return nullptr;
	Cell* cell = mCells[row][col];
	return cell ? mEntityRegistry.GetPlant(cell->GetNormalPlantID()) : nullptr;
}

Plant* Board::GetPumpkinAt(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) return nullptr;
	Cell* cell = mCells[row][col];
	return cell ? mEntityRegistry.GetPlant(cell->GetPumpkinPlantID()) : nullptr;
}

Plant* Board::GetOverlayPlantAt(int row, int col) const
{
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) return nullptr;
	Cell* cell = mCells[row][col];
	return cell ? mEntityRegistry.GetPlant(cell->GetOverlayPlantID()) : nullptr;
}

/** 快照格内分层实体 ID 后逐一重新解析，允许回调安全结束植物生命周期。 */
void Board::ForEachActivePlantInCell(int row, int col,
	const std::function<void(Plant&)>& action)
{
	if (!action || row < 0 || row >= mRows || col < 0 || col >= mColumns) return;
	const Cell* cell = mCells[row][col];
	if (!cell) return;

	const std::array<int, 4> plantIDs = {
		cell->GetOverlayPlantID(),
		cell->GetPumpkinPlantID(),
		cell->GetNormalPlantID(),
		cell->GetUnderPlantID(),
	};
	for (const int plantID : plantIDs) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (!plant || !plant->IsActive() || plant->IsPreview()
			|| plant->IsSquished() || plant->mPlantHealth <= 0) {
			continue;
		}
		action(*plant);
	}
}

/** 先冻结同格目标集合与拦截语义，再逐层承伤，避免上层死亡改变本次命中集合。 */
WinterGroundImpactResponse Board::ApplyWinterGroundImpactToCell(int row, int col,
	WinterGroundImpactKind kind, int damage, DamageSource source)
{
	std::array<int, 4> plantIDs{};
	std::size_t plantCount = 0;
	ForEachActivePlantInCell(row, col, [&](Plant& plant) {
		plantIDs[plantCount++] = plant.mPlantID;
	});

	WinterGroundImpactResponse response;
	for (std::size_t i = 0; i < plantCount; ++i) {
		Plant* plant = mEntityRegistry.GetPlant(plantIDs[i]);
		if (!plant || !plant->IsActive() || plant->IsPreview()
			|| plant->IsSquished() || plant->mPlantHealth <= 0) {
			continue;
		}
		const WinterGroundImpactResponse candidate =
			plant->ResolveWinterGroundImpact(kind);
		if (candidate.intercepted) {
			response = candidate;
			break;
		}
	}

	if (damage <= 0) return response;
	for (std::size_t i = 0; i < plantCount; ++i) {
		Plant* plant = mEntityRegistry.GetPlant(plantIDs[i]);
		if (!plant || !plant->IsActive() || plant->IsPreview()
			|| plant->IsSquished() || plant->mPlantHealth <= 0) {
			continue;
		}
		plant->TakeWinterGroundImpactDamage(kind, damage, source);
	}
	return response;
}

/**
 * 在目标九宫格中稳定选择最近的活动南瓜头；南瓜本体只由自己承伤，避免外壳连锁保护。
 */
Plant* Board::FindPumpkinAreaProtector(const Plant& plant) const
{
	if (!plant.IsActive() || plant.mRow < 0 || plant.mRow >= mRows
		|| plant.mColumn < 0 || plant.mColumn >= mColumns) {
		return nullptr;
	}

	if (plant.mPlantType == PlantType::PLANT_PUMPKINSHELL) {
		Plant* pumpkin = GetPumpkinAt(plant.mRow, plant.mColumn);
		return pumpkin && pumpkin->IsActive()
			&& pumpkin->mPlantID == plant.mPlantID ? pumpkin : nullptr;
	}

	Plant* best = nullptr;
	int bestDistanceSquared = INT_MAX;
	for (int row = std::max(0, plant.mRow - kPumpkinProtectionCellRadius);
		row <= std::min(mRows - 1, plant.mRow + kPumpkinProtectionCellRadius);
		++row) {
		for (int column = std::max(0,
			plant.mColumn - kPumpkinProtectionCellRadius);
			column <= std::min(mColumns - 1,
				plant.mColumn + kPumpkinProtectionCellRadius); ++column) {
			Plant* candidate = GetPumpkinAt(row, column);
			if (!candidate || !candidate->IsActive()) continue;

			const int rowDelta = row - plant.mRow;
			const int columnDelta = column - plant.mColumn;
			const int distanceSquared = rowDelta * rowDelta
				+ columnDelta * columnDelta;
			const bool stableTieBreak = best
				&& distanceSquared == bestDistanceSquared
				&& (candidate->mRow < best->mRow
					|| (candidate->mRow == best->mRow
						&& (candidate->mColumn < best->mColumn
							|| (candidate->mColumn == best->mColumn
								&& candidate->mPlantID < best->mPlantID))));
			if (!best || distanceSquared < bestDistanceSquared || stableTieBreak) {
				best = candidate;
				bestDistanceSquared = distanceSquared;
			}
		}
	}
	return best;
}

void Board::ApplyPumpkinProtectedZombieAreaDamage(int baseDamage,
	const std::function<bool(const Plant&)>& overlapsArea)
{
	ApplyPumpkinProtectedZombieAreaDamage(baseDamage,
		kPumpkinAreaDamageMultiplier, overlapsArea);
}

/**
 * 先按原范围收集命中层，再按九宫格保护者 ID 归并，避免密集或水路叠层重复扣壳。
 */
void Board::ApplyPumpkinProtectedZombieAreaDamage(int baseDamage,
	int pumpkinDamageMultiplier,
	const std::function<bool(const Plant&)>& overlapsArea)
{
	if (baseDamage <= 0 || pumpkinDamageMultiplier <= 0 || !overlapsArea) return;

	std::vector<int> unprotectedPlantIDs;
	std::unordered_set<int> protectedPumpkinIDSet;
	for (const int plantID : mEntityRegistry.GetAllPlantIDs()) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (!plant || !plant->IsActive() || !overlapsArea(*plant)) continue;

		if (Plant* pumpkin = FindPumpkinAreaProtector(*plant)) {
			protectedPumpkinIDSet.insert(pumpkin->mPlantID);
		}
		else {
			unprotectedPlantIDs.push_back(plantID);
		}
	}

	// 无外壳格保持旧行为：范围实际命中的 under/normal 各自吃一次基础伤害。
	for (const int plantID : unprotectedPlantIDs) {
		Plant* plant = mEntityRegistry.GetPlant(plantID);
		if (plant && plant->IsActive()) {
			plant->TakeDamage(baseDamage, DamageSource::ZOMBIE);
		}
	}

	const int pumpkinDamage = baseDamage > INT_MAX / pumpkinDamageMultiplier
		? INT_MAX : baseDamage * pumpkinDamageMultiplier;
	std::vector<int> protectedPumpkinIDs(
		protectedPumpkinIDSet.begin(), protectedPumpkinIDSet.end());
	std::sort(protectedPumpkinIDs.begin(), protectedPumpkinIDs.end());
	for (const int pumpkinID : protectedPumpkinIDs) {
		Plant* pumpkin = mEntityRegistry.GetPlant(pumpkinID);
		if (pumpkin && pumpkin->IsActive()) {
			pumpkin->TakeDamage(pumpkinDamage, DamageSource::ZOMBIE);
		}
	}
}

void Board::RefreshPlantStackRenderOrder(Cell* cell)
{
	if (!cell) return;
	Plant* under = mEntityRegistry.GetPlant(cell->GetUnderPlantID());
	Plant* normal = mEntityRegistry.GetPlant(cell->GetNormalPlantID());
	Plant* pumpkin = mEntityRegistry.GetPlant(cell->GetPumpkinPlantID());
	Plant* overlay = mEntityRegistry.GetPlant(cell->GetOverlayPlantID());
	std::vector<int> orders;
	if (under) orders.push_back(under->GetRenderOrder());
	if (normal) orders.push_back(normal->GetRenderOrder());
	if (pumpkin) orders.push_back(pumpkin->GetRenderOrder());
	if (overlay) orders.push_back(overlay->GetRenderOrder());
	if (orders.size() < 2) return;
	std::sort(orders.begin(), orders.end());
	size_t index = 0;
	if (under) under->SetRenderOrder(orders[index++]);
	if (normal) normal->SetRenderOrder(orders[index++]);
	if (pumpkin) pumpkin->SetRenderOrder(orders[index++]);
	if (overlay) overlay->SetRenderOrder(orders[index]);
}

Plant* Board::CreatePlant(PlantType plantType, int row, int column,
	bool skipsettings, bool isPreview)
{
	return CreatePlantInternal(plantType, plantType, row, column,
		skipsettings, isPreview);
}

Plant* Board::CreateImitaterPlant(PlantType targetType, int row, int column)
{
	if (targetType == PlantType::PLANT_IMITATER
		|| IsUpgradePlantType(targetType)
		|| !GameDataManager::GetInstance().HasPlant(targetType)) {
		return nullptr;
	}
	return CreatePlantInternal(PlantType::PLANT_IMITATER, targetType,
		row, column, false, false);
}

Plant* Board::CreatePlantInternal(PlantType actualType, PlantType placementType,
	int row, int column, bool skipsettings, bool isPreview)
{
	const int requestedRow = row;
	const int requestedColumn = column;
	if (!isPreview && !skipsettings
		&& !ResolvePlantPlacementAnchor(placementType, requestedRow, requestedColumn,
			row, column)) {
		return nullptr;
	}
	// 检查行列是否有效
	if (row < 0 || row >= mRows || column < 0 || column >= mColumns) {
		LOG_ERROR("Board") << "无效的行列位置: (" << row << ", " << column << ")";
		return nullptr;
	}

	// 正式创建入口也执行累计次数闸门，覆盖 AutoTest/develop 等绕过 CanPlantAt 的调用者。
	// 读档实体恢复由已保存的累计计数约束，不能在逐株重建时重复消耗次数。
	const bool consumesPlantingQuota = !isPreview && !skipsettings && !mIsLoadSave;
	if (consumesPlantingQuota && !HasPlantingQuota(placementType)) {
		return nullptr;
	}
	if (consumesPlantingQuota
		&& IsPlantFootprintFrozen(placementType, row, column)) {
		return nullptr;
	}
	if (consumesPlantingQuota) {
		const PlantFootprint footprint = GetPlantFootprint(placementType);
		for (std::size_t i = 0; i < footprint.count; ++i) {
			if (HasSnowHoleAt(row + footprint.cells[i].rowOffset,
				column + footprint.cells[i].columnOffset)) return nullptr;
		}
	}
	const bool isOverlayPlant = placementType == PlantType::PLANT_INSTANT_COFFEE;
	const bool isUpgradePlant = IsUpgradePlantType(placementType);
	if (isOverlayPlant && consumesPlantingQuota
		&& !CanPlantAt(placementType, row, column)) {
		return nullptr;
	}

	Plant* upgradeBasePlant = nullptr;
	std::vector<Plant*> upgradeBasePlants;
	bool inheritedSleeping = false;
	float inheritedWakeUpTimer = 0.0f;
	if (isUpgradePlant && !isPreview && !skipsettings) {
		if (!CanPlantAt(placementType, row, column)) return nullptr;
		upgradeBasePlant = GetUpgradePlantLayer(placementType) == PlantUpgradeLayer::UNDER
			? GetUnderPlantAt(row, column) : GetNormalPlantAt(row, column);
		if (!upgradeBasePlant) return nullptr;
		upgradeBasePlants.push_back(upgradeBasePlant);
		if (placementType == PlantType::PLANT_COBCANNON) {
			Plant* rearKernel = GetNormalPlantAt(row, column + 1);
			if (!rearKernel || rearKernel == upgradeBasePlant) return nullptr;
			upgradeBasePlants.push_back(rearKernel);
		}
		if (GetUpgradePlantLayer(placementType) == PlantUpgradeLayer::NORMAL) {
			inheritedSleeping = upgradeBasePlant->GetSleepState();
			inheritedWakeUpTimer = upgradeBasePlant->GetWakeUpTimeRemaining();
		}
	}

	// 根据植物类型创建对应的植物
	std::shared_ptr<Plant> plant = GameAPP::GetInstance().InstantiatePlant(
		actualType, this, row, column, isPreview);
	if (auto imitater = std::dynamic_pointer_cast<Imitater>(plant)) {
		imitater->SetImitaterTarget(placementType);
	}

	if (plant && !isPreview && !skipsettings) {
		Cell* cell = GetCell(row, column);
		const bool isUnderPlant = IsUnderPlantLayerType(placementType);
		const bool isPumpkinPlant = placementType == PlantType::PLANT_PUMPKINSHELL;
		const int occupiedID = isUnderPlant
			? cell->GetUnderPlantID()
			: (isPumpkinPlant ? cell->GetPumpkinPlantID()
				: (isOverlayPlant ? cell->GetOverlayPlantID() : cell->GetNormalPlantID()));
		const bool replacesExpectedBase = isUpgradePlant && upgradeBasePlant
			&& occupiedID == upgradeBasePlant->mPlantID;
		if (occupiedID != NULL_PLANT_ID && !replacesExpectedBase) {
			plant->Die();
			return nullptr;
		}
		mEntityRegistry.AddPlant(plant);

		// 将植物与格子关联
		if (isUnderPlant) cell->SetUnderPlantID(plant->mPlantID);
		else if (isPumpkinPlant) cell->SetPumpkinPlantID(plant->mPlantID);
		else if (isOverlayPlant) cell->SetOverlayPlantID(plant->mPlantID);
		else {
			std::vector<int> replacePlantIDs;
			replacePlantIDs.reserve(upgradeBasePlants.size());
			for (Plant* base : upgradeBasePlants) {
				if (base) replacePlantIDs.push_back(base->mPlantID);
			}
			if (!OccupyPlantFootprint(placementType, row, column,
				plant->mPlantID, replacePlantIDs)) {
				plant->Die();
				return nullptr;
			}
		}
		if (replacesExpectedBase) {
			// 先把格子切到新 ID，再让旧株死亡；ReleaseGridSlot 只清自己的 ID，因此替换原子化。
			for (Plant* base : upgradeBasePlants) {
				if (base) base->Die();
			}
			if (actualType != PlantType::PLANT_IMITATER
				&& GetUpgradePlantLayer(placementType) == PlantUpgradeLayer::NORMAL
				&& inheritedSleeping) {
				plant->SetSleepState(true);
				plant->RestoreSleepState(true, inheritedWakeUpTimer);
			}
			else if (actualType != PlantType::PLANT_IMITATER
				&& GetUpgradePlantLayer(placementType) == PlantUpgradeLayer::NORMAL) {
				plant->SetSleepState(false);
			}
		}
		const PlantFootprint footprint = GetPlantFootprint(placementType);
		for (std::size_t i = 0; i < footprint.count; ++i) {
			if (Cell* occupiedCell = GetCell(
				row + footprint.cells[i].rowOffset,
				column + footprint.cells[i].columnOffset)) {
				RefreshPlantStackRenderOrder(occupiedCell);
			}
		}
		if (placementType == PlantType::PLANT_ELITE_SCAREDYSHROOM && consumesPlantingQuota) {
			++mEliteScaredyShroomsPlanted;
		}
		if (placementType == PlantType::PLANT_PLANTERN) {
			mActivePlanternID = plant->mPlantID;
		}
	}

	return plant.get();
}

Plant* Board::MorphImitater(Imitater* imitater)
{
	if (!imitater || !imitater->IsActive() || imitater->mBoard != this
		|| !imitater->HasValidTarget()) {
		return nullptr;
	}

	const PlantType targetType = imitater->GetImitaterTarget();
	const int plantID = imitater->mPlantID;
	const int row = imitater->mRow;
	const int column = imitater->mColumn;
	std::shared_ptr<Plant> replacement = GameAPP::GetInstance().InstantiatePlant(
		targetType, this, row, column, false);
	if (!replacement) return nullptr;

	// Cell 继续保存同一个 ID；先覆盖注册表，再静默回收旧对象，避免 ReleaseGridSlot
	// 把刚刚移交给目标植物的层或双格 footprint 清空。
	mEntityRegistry.AddPlantWithID(replacement, plantID);
	replacement->SetImitatedAppearance(true);
	if (auto* blover = dynamic_cast<Blover*>(replacement.get())) {
		blover->SetBlowDirection(imitater->GetInheritedBloverDirection());
	}
	if (targetType == PlantType::PLANT_PLANTERN) {
		mActivePlanternID = plantID;
	}
	const PlantFootprint footprint = GetPlantFootprint(targetType);
	for (std::size_t i = 0; i < footprint.count; ++i) {
		RefreshPlantStackRenderOrder(GetCell(
			row + footprint.cells[i].rowOffset,
			column + footprint.cells[i].columnOffset));
	}
	imitater->RetireAfterReplacement();
	return replacement.get();
}

Zombie* Board::CreateZombie(ZombieType zombieType, int row, float x, bool skipsettings, bool isPreview) {
	// y 由 row 与地形上的当前 x 共同决定；屋顶出生点因此直接落在连续坡面上。
	float y = GetZombieSpawnY(row, x);
	if (y < 0.0f) y = 0.0f;

	std::shared_ptr<Zombie> zombie = GameAPP::GetInstance().InstantiateZombie
	(zombieType, this, x, y, row, isPreview);
	if (!zombie) return nullptr;

	mZombieNumber++;

	if (!isPreview && !skipsettings) {
		mEntityRegistry.AddZombie(zombie);
		zombie->mSpawnWave = this->mCurrentWave;
		// 按当前难度来源对整只僵尸血量施加全局倍率（默认 1，目前由生存模式按轮次提供）。
		// 仅在此波次生成路径施加；读档走 CreateZombieWithID 直接还原已含倍率的存档血量，不重复缩放。
		zombie->ApplyHealthMultiplier(GetZombieHpMultiplier());
		// 词条②：按当前词条层数设定出生免伤次数（无词条→0）。读档走 CreateZombieWithID 不在此赋值，
		// 由 LoadProtectedData 还原（与血量倍率同契约）。
		zombie->mFreeHitsRemaining = GetPerkManager().GetZombieInvulnHits();
		zombie->PlaySpawnSound();
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

	// 从对象池获取子弹（shared_ptr 局部变量，用于把 weak_ptr 注册进 EntityRegistry）
	std::shared_ptr<Bullet> bullet = bulletPool->AcquireShared
	(this, bulletType, row, Vector(10, 10), position);

	if (bullet && !skipsettings) {
		mEntityRegistry.AddBullet(bullet);
	}

	return bullet.get();
}

inline void Board::CleanupExpiredObjects()
{
	// 清理已过期的植物ID映射
	// TODO 如果其他地方也有存储植物ID,也要删除
	std::vector<int> removedPlants = mEntityRegistry.CleanupExpired();

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
			if (mCells[i][j]->GetPumpkinPlantID() == plantID)
				mCells[i][j]->ClearPumpkinPlantID();
			if (mCells[i][j]->GetOverlayPlantID() == plantID)
				mCells[i][j]->ClearOverlayPlantID();
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

/** 用环境小阳光补偿泳池六路防线与睡莲成本，不占用玩家卡槽。 */
inline void Board::UpdatePoolSunFalling(float deltaTime)
{
	mPoolSunCountDown -= deltaTime;
	if (mPoolSunCountDown > 0.0f) return;

	mPoolSunCountDown = POOL_SUN_SPAWN_TIME;
	const int row = GameRandom::Range(2, 3);
	const int column = GameRandom::Range(0, mColumns - 1);
	Vector sunPos = GetCellCenterPosition(row, column);
	sunPos.x += GameRandom::Range(-20.0f, 20.0f);
	sunPos.y -= 20.0f;
	CreateSmallSun(sunPos, true);
}

void Board::UpdateLevel()
{
	if (mBoardState != BoardState::GAME) return;
	float deltaTime = DeltaTime::GetDeltaTime();

	if (mBackGround == Background::GROUND_DAY || mBackGround == Background::WATER_POOL ||
		mBackGround == Background::ROOF) {
		UpdateSunFalling(deltaTime);
	}
	if (mBackGround == Background::WATER_POOL || mBackGround == Background::NIGHT_WATER_POOL) {
		UpdatePoolSunFalling(deltaTime);
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
			for (int id : mEntityRegistry.GetAllPlantIDs())
			{
				Plant* p = mEntityRegistry.GetPlant(id);
				if (!p || p->IsPreview() || p->IsIceSealed()) continue;
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

	if (mCurrentWave > 0 && mPendingSnowHoleSpawns.empty()
		&& mCurrectWaveZombieHP <= mNextWaveSpawnZombieHP)
	{
		mZombieCountDown = 0.0f;
	}

	if (mZombieCountDown <= 0.0f)
	{
		// 一大波僵尸处理
		if ((mCurrentWave + 1) % 10 == 0)
		{
			if (mCurrentWave + 1 == mMaxWave) BeginPolarFinalWavePrelude();
			mHugeWaveCountDown += deltaTime;
			if (!mHasHugeWaveSound)
			{
				mHasHugeWaveSound = true;
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HUGEWAVE, 0.7f);
				if (mPresentation)
					mPresentation->ShowPrompt(
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
	mCurrentWave++;
	if (mCurrentWave == mMaxWave
		&& SupportsPolarNightEnvironment()
		&& AdventureProgression::GetLevelNumberInArea(mLevel) == 9
		&& !mPolarFinalWaveUpgradeApplied) {
		// 开发者直调跳过大波警告时仍保证最终波处于白毛风；正常流程走平滑预热分支。
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
	// 在本波任何候选解析前承接冷却，保证整个波次（含稍后的显式正式候选）都保持封锁。
	AdvanceHijackerSpawnCooldownForNewWave();
	mZombieCountDown = IsStormyNightActive()
		? kStormyNightNextWaveSeconds : NEXTWAVE_COUNT_MAX;
	if (IsStormyNightActive() && !mStormyNightInitialized) {
		ActivateStormyNight();
	}
	// 普通关压力随波次推进；先刷新存活僵尸，再生成使用同一新倍率的本波僵尸。
	RefreshZombieWeatherSpeeds();
	mEliteDancersSpawnedThisWave = 0;
	mReinforcedDoorsSpawnedThisWave = 0;
	mElitePolevaultersSpawnedThisWave = 0;
	mGildedZambonisSpawnedThisWave = 0;
	mEliteDolphinRidersSpawnedThisWave = 0;
	mEliteJackInTheBoxesSpawnedThisWave = 0;
	mEliteDiggersSpawnedThisWave = 0;
	mElitePogosSpawnedThisWave = 0;
	mEliteLaddersSpawnedThisWave = 0;
	mEliteCatapultsSpawnedThisWave = 0;
	mRedeyeGargantuarsSpawnedThisWave = 0;
	mInsulatorsSpawnedThisWave = 0;
	mHijackersSpawnedThisWave = 0;
	mGroundingZombiesSpawnedThisWave = 0;
	mBobsledTeamsSpawnedThisWave = 0;
	mIceWallEngineersSpawnedThisWave = 0;
	mIceCrackDrillsSpawnedThisWave = 0;
	mWeatherJammersSpawnedThisWave = 0;
	mIceStatueExecutionersSpawnedThisWave = 0;
	mSnowBurrowsSpawnedThisWave = 0;
	mAdaptiveHelmetsSpawnedThisWave = 0;
	mMistFuelAssignedThisWave = 0;
	if (mCurrentWave == 1)
	{
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FIRSTWAVE, 0.7f);
		if (mPresentation) mPresentation->ActivateWaveProgress();
	}
	if (mCurrentWave == mMaxWave)
	{
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FINALWAVE, 0.7f);
		if (mPresentation)
			mPresentation->ShowPrompt(
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
	TrySummonAdventureBoss();
	UpdateZombieMetrics();

	mNextWaveSpawnZombieHP = static_cast<int64_t>
		(GameRandom::Range(0.5f, 0.65f) * static_cast<double>(mCurrectWaveZombieHP));
}

/**
 * 关卡编排只在准确的最终波读取显式 BOSS 槽位；越过最终波的开发者直调不会重复创建。
 */
void Board::TrySummonAdventureBoss()
{
	if (mCurrentWave != mMaxWave || mCurrentWave <= 0) return;

	switch (AdventureProgression::GetBossSlot(mLevel)) {
	case AdventureProgression::BossSlot::ROOF_MARSHAL:
		// 固定中路便于玩家识别首领入场；Y 仍由 CreateZombie 按屋顶坡面统一解析。
		CreateZombie(ZombieType::ZOMBIE_ROOF_MARSHAL, mRows / 2,
			kRoofMarshalBossSpawnX);
		break;
	case AdventureProgression::BossSlot::NONE:
		break;
	}
}

/** 为每个出怪候选创建选卡展示对象，并为编队候选补充无玩法注册的纯展示成员。 */
void Board::CreatePreviewZombies()
{
	if (mBoardState != BoardState::CHOOSE_CARD || !mPreviewZombieList.empty()
		|| mSpawnZombieList.empty()) return;

	mPreviewZombieList.clear();
	for (ZombieType zombieType : mSpawnZombieList)
	{
		// 选卡镜头会平移到扩展世界区域；屋顶使用主人实测的专属世界坐标范围。
		const float spawnX = IsRoofBackground()
			? GameRandom::Range(kRoofPreviewZombieMinX, kRoofPreviewZombieMaxX)
			: GameRandom::Range(mSpawnZombiePos1.x, mSpawnZombiePos2.x);
		std::shared_ptr<Zombie> preview;
		if (IsRoofBackground()) {
			// 屋顶预览仍需占一条视觉路径，才能消费连续坡面；其他地图保留自由散落布局。
			const int row = GameRandom::Range(
				std::min(kRoofPreviewZombieFirstRow, mRows - 1), mRows - 1);
			preview = GameAPP::GetInstance().InstantiateZombie(
				zombieType, this, spawnX, GetZombieSpawnY(row, spawnX), row, true);
		}
		else {
			const float spawnY = GameRandom::Range(mSpawnZombiePos1.y, mSpawnZombiePos2.y);
			preview = GameAPP::GetInstance().InstantiateZombieFree(
				zombieType, this, spawnX, spawnY);
		}
		if (!preview) continue;
		auto addPreview = [this](const std::shared_ptr<Zombie>& member) {
			if (!member) return;
			// 与 Zombie::Die 中的 mZombieNumber-- 保持平衡（预览僵尸 board == this，销毁时会递减）。
			mZombieNumber++;
			mPreviewZombieList.push_back(member.get());
		};

		if (auto* leader = dynamic_cast<BobsledTeamZombie*>(preview.get())) {
			// 出怪表仍只有一个正式候选；选卡宣传画面单独补齐三名无 ID、无碰撞的展示队员。
			leader->ConfigurePreviewTeamMember(0);
			addPreview(preview);
			const Vector leaderPosition = preview->GetPosition();
			for (int slot = 1; slot < 4; ++slot) {
				auto member = GameAPP::GetInstance().InstantiateZombieFree(
					zombieType, this,
					leaderPosition.x + BobsledTeamZombie::GetPreviewMemberOffsetX(slot),
					leaderPosition.y);
				if (member) addPreview(member);
				if (auto* rider = dynamic_cast<BobsledTeamZombie*>(member.get())) {
					rider->ConfigurePreviewTeamMember(slot);
				}
			}
			continue;
		}

		addPreview(preview);
	}
}

Bullet* Board::CreatePlantBullet(BulletType bulletType, int row,
	const Vector& position, PlantType originPlant)
{
	Bullet* bullet = CreateBullet(bulletType, row, position);
	if (bullet) {
		bullet->SetPlantDamageOrigin(PlantDamageOrigin::FromPlant(originPlant));
	}
	return bullet;
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
	// 海豚僵尸依赖池沿入水状态机，只能在泳池背景的两条水路生成。
	if (type == ZombieType::ZOMBIE_DOLPHIN_RIDER
		|| type == ZombieType::ZOMBIE_ELITE_DOLPHIN_RIDER) {
		return IsPoolBackground() && row >= 0 && row < mRows && IsPoolRow(row);
	}
	// 普通冰车已支持昼/夜屋顶的连续坡面；鎏金冰车仍保持未解禁状态。
	if (IsRoofBackground() && type == ZombieType::ZOMBIE_GILDED_ZAMBONI) return false;
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
		return -1;
	}

	float randNum = GameRandom::Range(0.0f, smoothTotal);
	float cumulative = 0.0f;
	int lastCompatibleRow = -1;
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

	float x = static_cast<float>(SCENE_WIDTH) + 40;
	// 6-4 第七波是劫持者的整波单体教学，不受正常第九波出怪门槛限制。
	if (!mIsSurvival && mLevel == kHijackerTutorialLevel
		&& mCurrentWave == kHijackerTutorialWave) {
		const ZombieType actualType = ResolveWaveZombieType(ZombieType::ZOMBIE_HIJACKER);
		if (actualType != ZombieType::NUM_ZOMBIE_TYPES) {
			const int row = SelectSpawnRow(actualType);
			if (row >= 0) {
				CreateOrQueueWaveZombie(actualType, row, x);
			}
		}
		return;
	}
	// 6-6 第三波额外保底一只急救员；它不消耗正常预算，且普通池仍可自然再选中急救员。
	if (!mIsSurvival && mLevel == kHealerTutorialLevel
		&& mCurrentWave == kHealerTutorialWave) {
		const ZombieType actualType = ResolveWaveZombieType(ZombieType::ZOMBIE_HEALER);
		if (actualType != ZombieType::NUM_ZOMBIE_TYPES) {
			const int row = SelectSpawnRow(actualType);
			if (row >= 0) {
				CreateOrQueueWaveZombie(actualType, row, x);
			}
		}
	}
	// 7-6 第三波额外保底一只气象干扰僵尸；不消耗预算，但仍消费本波唯一名额。
	if (!mIsSurvival && mLevel == kWeatherJammerTutorialLevel
		&& mCurrentWave == kWeatherJammerTutorialWave) {
		const ZombieType actualType = ResolveWaveZombieType(
			ZombieType::ZOMBIE_WEATHER_JAMMER);
		if (actualType != ZombieType::NUM_ZOMBIE_TYPES) {
			const int row = SelectSpawnRow(actualType);
			if (row >= 0) {
				CreateOrQueueWaveZombie(actualType, row, x);
			}
		}
	}
	// 7-7 第三波额外保底一只冰像处刑者；不消耗预算，但仍消费本波唯一名额。
	if (!mIsSurvival && mLevel == kIceExecutionerTutorialLevel
		&& mCurrentWave == kIceExecutionerTutorialWave) {
		const ZombieType actualType = ResolveWaveZombieType(
			ZombieType::ZOMBIE_ICE_STATUE_EXECUTIONER);
		if (actualType != ZombieType::NUM_ZOMBIE_TYPES) {
			const int row = SelectSpawnRow(actualType);
			if (row >= 0) {
				CreateOrQueueWaveZombie(actualType, row, x);
			}
		}
	}
	// 8-1 的雪穴形成后，在首个可用波次锁定一只潜雪僵尸；封穴使本次退回右侧且不消费全关保底。
	if (!mIsSurvival && mLevel == kSnowBurrowTutorialLevel
		&& !mSnowBurrowTutorialHoleSpawnConsumed) {
		int tutorialRow = -1;
		for (int row = 0; row < std::min(mRows,
			static_cast<int>(mSnowHoles.size())); ++row) {
			if (mSnowHoles[row].phase == SnowHolePhase::ACTIVE) {
				tutorialRow = row;
				break;
			}
		}
		if (tutorialRow >= 0) {
			const ZombieType actualType = ResolveWaveZombieType(
				ZombieType::ZOMBIE_SNOW_BURROW);
			if (actualType != ZombieType::NUM_ZOMBIE_TYPES) {
				CreateOrQueueWaveZombie(actualType, tutorialRow, x, true);
			}
		}
	}

	int remainingPoints = CalculateWaveZombiePoints();
	int zombiesSpawned = 0;
	int candidatesExamined = 0;

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
		// 地图没有任何合法行时跳过候选，禁止把地形不兼容品种静默塞进第 1 路。
		if (row < 0) continue;

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

		if (CreateOrQueueWaveZombie(actualType, row, x))
		{
			zombiesSpawned++;
			remainingPoints -= cost;
		}
	}
}

/** 返回本关雾火经济从首波宽松供给到最终波紧缩供给的平滑进度。 */
float Board::GetMistFuelScarcityFactor() const
{
	if (mMaxWave <= 1) return 0.0f;
	const float linear = std::clamp(
		static_cast<float>(mCurrentWave - 1) / static_cast<float>(mMaxWave - 1),
		0.0f, 1.0f);
	return linear * linear * (3.0f - 2.0f * linear);
}

int Board::GetMistFuelRewardAmount() const
{
	return static_cast<int>(std::lround(LerpWeatherValue(
		static_cast<float>(kMistFuelEarlyRewardAmount),
		static_cast<float>(kMistFuelLateRewardAmount),
		GetMistFuelScarcityFactor())));
}

int Board::GetMistFuelWaveBudget() const
{
	return GetMistFuelRewardAmount() * kMistFuelCarriersPerWave;
}

float Board::GetMistFuelBaseCarrierChance() const
{
	return LerpWeatherValue(kMistFuelEarlyBaseCarrierChance,
		kMistFuelLateBaseCarrierChance, GetMistFuelScarcityFactor());
}

float Board::GetMistFuelHeavyCarrierBonus() const
{
	return LerpWeatherValue(kMistFuelEarlyHeavyCarrierBonus,
		kMistFuelLateHeavyCarrierBonus, GetMistFuelScarcityFactor());
}

/**
 * 只给正式波次出生分配雾火：首波提高命中以缓解低出怪量断供，之后独立于天气压力逐波
 * 压低单团价值、携带概率和单波预算；未命中的份额仍跨波累计，避免长期完全断供。
 */
void Board::AssignMistFuelReward(Zombie* zombie)
{
	if (!zombie || !SupportsPlanternMechanics()) return;
	const int rewardAmount = GetMistFuelRewardAmount();
	if (mMistFuelAssignedThisWave + rewardAmount > GetMistFuelWaveBudget()) return;

	const int totalMaxHealth = std::max(0, zombie->mBodyMaxHealth)
		+ std::max(0, zombie->mHelmMaxHealth)
		+ std::max(0, zombie->mShieldMaxHealth);
	const float heavyFactor = std::clamp(
		(static_cast<float>(totalMaxHealth) - 270.0f) / 1800.0f, 0.0f, 1.0f);
	mMistFuelDropAccumulator += GetMistFuelBaseCarrierChance()
		+ GetMistFuelHeavyCarrierBonus() * heavyFactor;

	const float hitChance = std::min(mMistFuelDropAccumulator, 1.0f);
	if (GameRandom::Range(0.0f, 1.0f) > hitChance) return;
	mMistFuelDropAccumulator = std::max(
		0.0f, mMistFuelDropAccumulator - 1.0f);
	mMistFuelAssignedThisWave += rewardAmount;
	zombie->SetMistFuelReward(static_cast<float>(rewardAmount));
}

inline int Board::CalculateWaveZombiePoints() const
{
	// 基础点数
	float points = (static_cast<float>(mCurrentWave) / 3 + 1.0f) * 1000.0f;

	points *= (GameAPP::GetInstance().Difficulty * 0.5f);
	if (IsStormyNightActive()) {
		points *= kStormyNightWavePointMultiplier;
	}

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

int Board::GetCurrentWaveZombiePoints() const
{
	return CalculateWaveZombiePoints();
}

inline void Board::UpdateZombieMetrics()
{
	int64_t TotalHP = 0, CurrectWaveHP = 0;
	int hostileZombieCountForMusic = 0;
	for (auto zombieID : mEntityRegistry.GetAllZombieIDs())
	{
		if (auto zombie = mEntityRegistry.GetZombie(zombieID))
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
	// 固定步预算不依赖渲染帧；间隔覆盖一次最多三步的追帧，避免同一渲染帧重叠推演。
	if (DeltaTime::GetDeltaTime() > 0.0f
		&& mMonteCarloHealerDecisionCooldownSteps > 0) {
		--mMonteCarloHealerDecisionCooldownSteps;
	}
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
	// 夜间泳池迷雾与雨势正交，但同样使用游戏时间并消费更新后的台风强度和实时风向。
	UpdateFog(DeltaTime::GetDeltaTime());
	UpdateWeatherPanelInterference(DeltaTime::GetDeltaTime());
	UpdateIceTrails(DeltaTime::GetDeltaTime());
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
	if (mPresentation) {
		mPresentation->ShowShovel();
	}
	if (!mIsLoadSave) {
		InitializeMowers();
	}
	mBoardState = BoardState::GAME;
	InitializeWeather();
	InitializeWinterTemperature();
	InitializePolarNightEnvironment();
	InitializeFogWeather();
	EnforceStormyNightWeather();
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

/**
 * 按 C# CutScene.AddFlowerPots 的列优先顺序，为新开的屋顶冒险关铺设初始花盆。
 * 当前九关制把原版 5-1/5-2/后续屋顶关的 5/4/3 列规则映射到内部 37/38/39～54。
 */
void Board::InitializeStartingFlowerPots()
{
	if (!IsRoofBackground()) return;

	int columnCount = 3;
	if (AdventureProgression::IsAdventureLevel(mLevel)
		&& AdventureProgression::GetAreaNumber(mLevel) == 5) {
		const int levelInArea = AdventureProgression::GetLevelNumberInArea(mLevel);
		if (levelInArea == 1) columnCount = 5;
		else if (levelInArea == 2) columnCount = 4;
	}

	// 原版外层遍历列、内层遍历行；保留创建顺序，令实体 ID 与演出层叠同源可复现。
	for (int column = 0; column < std::min(columnCount, mColumns); ++column) {
		for (int row = 0; row < mRows; ++row) {
			if (CanPlantAt(PlantType::PLANT_FLOWERPOT, row, column)) {
				CreatePlant(PlantType::PLANT_FLOWERPOT, row, column);
			}
		}
	}
}

void Board::PrepareBackgroundMusic()
{
	AudioSystem::PrepareMusic(BackgroundMusicKey(mBackGround));
}

void Board::PlayBackgroundMusic()
{
	AudioSystem::PlayMusic(BackgroundMusicKey(mBackGround), -1);
}

void Board::GameOver()
{
	DeltaTime::SetPaused(true);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LOSTGAME, 0.65f);
	AudioSystem::StopMusic();
	if (mPresentation)
		mPresentation->GameOver();
	mBoardState = BoardState::LOSE_GAME;
}

void Board::OnSurvivalRoundClear()
{
	if (!mIsSurvival) return;
	for (int plantID : mEntityRegistry.GetAllPlantIDs()) {
		if (Plant* plant = mEntityRegistry.GetPlant(plantID);
			plant && plant->IsActive() && !plant->IsSquished()) {
			plant->ResetUnyieldingRootsForRound();
		}
	}

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
	mGildedZambonisSpawnedThisWave = 0;
	mEliteDolphinRidersSpawnedThisWave = 0;
	mEliteJackInTheBoxesSpawnedThisWave = 0;
	mEliteDiggersSpawnedThisWave = 0;
	mElitePogosSpawnedThisWave = 0;
	mEliteLaddersSpawnedThisWave = 0;
	mEliteCatapultsSpawnedThisWave = 0;
	mRedeyeGargantuarsSpawnedThisWave = 0;
	mInsulatorsSpawnedThisWave = 0;
	mHijackersSpawnedThisWave = 0;
	mGroundingZombiesSpawnedThisWave = 0;
	mBobsledTeamsSpawnedThisWave = 0;
	mIceWallEngineersSpawnedThisWave = 0;
	mIceCrackDrillsSpawnedThisWave = 0;
	mWeatherJammersSpawnedThisWave = 0;
	mIceStatueExecutionersSpawnedThisWave = 0;
	mSnowBurrowsSpawnedThisWave = 0;
	mAdaptiveHelmetsSpawnedThisWave = 0;
	mAdaptiveHelmetTutorialWaveSpawned = false;
	RefreshZombieWeatherSpeeds();

	// 重算难度（解锁更强僵尸）+ 刷新关卡名
	BuildSurvivalSpawnList(mSurvivalRound);
	UpdateSurvivalLevelName();

	// 回到选卡：暂停波次推进
	mBoardState = BoardState::CHOOSE_CARD;

	// 通知场景：先结算两次成对词条机会（每次可选或放弃），然后再链式进入选卡。
	if (mPresentation)
		mPresentation->BeginSurvivalPerkSelect();
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

/** 按无尽轮次、地形和类型资格重建本轮可见僵尸卡池。 */
void Board::BuildSurvivalSpawnList(int round)
{
	mSpawnZombieList.clear();
	mSpawnZombieList.push_back(ZombieType::ZOMBIE_NORMAL);   // 普通：每轮必有，先固定放入

	// 1) 收集本轮合格候选(排除普通)。旗数递减下调每种僵尸的"有效最早轮次"。
	std::vector<ZombieType> candidates;
	for (int i = 0; i < static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES); ++i)
	{
		const ZombieType t = static_cast<ZombieType>(i);
		if (t == ZombieType::ZOMBIE_NORMAL) continue;
		if (CanZombieTypeEnterSurvivalPool(t, round)) candidates.push_back(t);
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

/** 统一无尽僵尸卡池资格；直造、冒险出怪和其他召唤来源不受这里影响。 */
bool Board::CanZombieTypeEnterSurvivalPool(ZombieType type, int round) const
{
	const int typeIndex = static_cast<int>(type);
	if (!mIsSurvival || round < 1 || typeIndex < 0
		|| typeIndex >= static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES)) return false;
	if (type == ZombieType::ZOMBIE_NORMAL) return true;
	if (type == ZombieType::ZOMBIE_GILDED_ZAMBONI) return false;

	// 粉色橄榄球是黑夜专属变体；冒险关仍只由 spawnlists.json 控制。
	if (type == ZombieType::ZOMBIE_PINK_FOOTBALL
		&& !GameAPP::GetInstance().GetBackgroundIsNight(mBackGround)) return false;
	const auto& gameData = GameDataManager::GetInstance();
	const int baseRound = gameData.GetZombieSurvivalRound(type);
	if (baseRound < 1 || gameData.GetZombieWeight(type) <= 0) return false;

	bool hasCompatibleRow = false;
	for (int row = 0; row < mRows; ++row) {
		if (!IsSpawnRowCompatible(type, row)) continue;
		hasCompatibleRow = true;
		break;
	}
	if (!hasCompatibleRow) return false;

	const int flagsCompleted = round - 1;
	const int reduction = SurvivalCurveLerp(SURVIVAL_UNLOCK_REDUCE_START_FLAG,
		SURVIVAL_UNLOCK_REDUCE_END_FLAG, flagsCompleted, 0, SURVIVAL_UNLOCK_REDUCE_MAX);
	return std::max(baseRound - reduction, 1) <= round;
}

void Board::UpdateSurvivalLevelName()
{
	const auto* definition = FindSurvivalEndlessDefinition(mLevel);
	const std::string label = definition ? definition->label : u8"未知无尽";
	mLevelName = std::string(u8"生存模式：") + label + u8" 第"
		+ std::to_string(mSurvivalRound) + u8"轮";
}

bool Board::ConsumePlantDamageEchoHit()
{
	if (!mPerkManager.HasPlantDamageEcho()) {
		mPlantDamageEchoHitCounter = 0;
		return false;
	}
	constexpr int kDamageEchoHitInterval = 10; // 每十次实际植物伤害命中触发一次同目标回响
	++mPlantDamageEchoHitCounter;
	if (mPlantDamageEchoHitCounter < kDamageEchoHitInterval) return false;
	mPlantDamageEchoHitCounter = 0;
	return true;
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
	return CreatePlantWithIDInternal(type, type, row, col, id);
}

Plant* Board::CreateImitaterPlantWithID(
	PlantType targetType, int row, int col, int id)
{
	if (targetType == PlantType::PLANT_IMITATER
		|| IsUpgradePlantType(targetType)
		|| !GameDataManager::GetInstance().HasPlant(targetType)) {
		return nullptr;
	}
	return CreatePlantWithIDInternal(
		PlantType::PLANT_IMITATER, targetType, row, col, id);
}

Plant* Board::CreatePlantWithIDInternal(PlantType actualType,
	PlantType placementType, int row, int col, int id)
{
	Cell* cell = GetCell(row, col);
	const bool isUnderPlant = IsUnderPlantLayerType(placementType);
	const bool isPumpkinPlant = placementType == PlantType::PLANT_PUMPKINSHELL;
	const bool isOverlayPlant = placementType == PlantType::PLANT_INSTANT_COFFEE;
	if (cell && (isUnderPlant
		? cell->GetUnderPlantID()
		: (isPumpkinPlant ? cell->GetPumpkinPlantID()
			: (isOverlayPlant ? cell->GetOverlayPlantID() : cell->GetNormalPlantID()))) != NULL_PLANT_ID) {
		return nullptr;
	}
	if (!isUnderPlant && !isPumpkinPlant && !isOverlayPlant) {
		const PlantFootprint footprint = GetPlantFootprint(placementType);
		for (std::size_t i = 0; i < footprint.count; ++i) {
			Cell* occupiedCell = GetCell(row + footprint.cells[i].rowOffset,
				col + footprint.cells[i].columnOffset);
			if (!occupiedCell || occupiedCell->GetNormalPlantID() != NULL_PLANT_ID) {
				return nullptr;
			}
		}
	}
	// 走 GameApp 工厂拿 shared_ptr 用于 EntityRegistry 注册
	if (row < 0 || row >= mRows || col < 0 || col >= mColumns) {
		LOG_ERROR("Board") << "无效的行列位置: (" << row << ", " << col << ")";
		return nullptr;
	}
	std::shared_ptr<Plant> plant = GameAPP::GetInstance().InstantiatePlant(
		actualType, this, row, col, false);
	if (auto imitater = std::dynamic_pointer_cast<Imitater>(plant)) {
		imitater->SetImitaterTarget(placementType);
	}
	if (plant) {
		mEntityRegistry.AddPlantWithID(plant, id);
		if (cell) {
			if (isUnderPlant) cell->SetUnderPlantID(id);
			else if (isPumpkinPlant) cell->SetPumpkinPlantID(id);
			else if (isOverlayPlant) cell->SetOverlayPlantID(id);
			else if (!OccupyPlantFootprint(placementType, row, col, id)) {
				plant->Die();
				return nullptr;
			}
			const PlantFootprint footprint = GetPlantFootprint(placementType);
			for (std::size_t i = 0; i < footprint.count; ++i) {
				if (Cell* occupiedCell = GetCell(row + footprint.cells[i].rowOffset,
					col + footprint.cells[i].columnOffset)) {
					RefreshPlantStackRenderOrder(occupiedCell);
				}
			}
		}
		if (placementType == PlantType::PLANT_PLANTERN) {
			mActivePlanternID = id;
		}
	}
	return plant.get();
}

Zombie* Board::CreateZombieWithID(ZombieType type, int row, float x, int id) {
	// y 由持久化的 row + x 重建，屋顶不需要新增坐标字段即可恢复连续坡面。
	float y = GetZombieSpawnY(row, x);
	if (y < 0.0f) y = 0.0f;

	std::shared_ptr<Zombie> zombie = GameAPP::GetInstance().InstantiateZombie
	(type, this, x, y, row, false);
	if (!zombie) return nullptr;
	mZombieNumber++;
	mEntityRegistry.AddZombieWithID(zombie, id);
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
		mEntityRegistry.AddBulletWithID(bullet, id);
	}
	return bullet.get();
}

Sun* Board::CreateSunWithID(const Vector& pos, bool fromSky, int id) {
	auto sun = GameObjectManager::GetInstance().CreateGameObjectAsShared<Sun>
		(LAYER_GAME_COIN, this, pos, 0.85f, "Sun",
			fromSky, true);
	if (sun) {
		mEntityRegistry.AddCoinWithID(sun, id);
	}
	return sun.get();
}

SmallSun* Board::CreateSmallSunWithID(const Vector& pos, bool fromSky, int id) {
	auto sun = GameObjectManager::GetInstance().CreateGameObjectAsShared<SmallSun>
		(LAYER_GAME_COIN, this, pos, 0.6f, "SmallSun",
			fromSky, true);
	if (sun) {
		mEntityRegistry.AddCoinWithID(sun, id);
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

bool Board::CanHaveMowerInRow(int row) const
{
	return row >= 0 && row < mRows
		&& (!SupportsWinterTemperature() || row >= 2);
}

/** 创建当前地形允许的小推车；冬日花园温室遮挡行直接返回空。 */
Mower* Board::CreateMower(MowerType type, int row)
{
	if (!CanHaveMowerInRow(row)) return nullptr;
	if (IsRoofBackground()) type = MowerType::ROOF;
	float x = 160.0f;
	float y = GetMowerTerrainY(row, x + 40.0f);
	const AnimationType animType = type == MowerType::WATER
		? AnimationType::ANIM_POOL_CLEANER
		: (type == MowerType::ROOF
			? AnimationType::ANIM_ROOF_CLEANER : AnimationType::ANIM_LAWNMOWER);
	const float scale = type == MowerType::WATER ? 0.8f : 0.85f;

	auto mower = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Mower>(
		LAYER_GAME_OBJECT, this, type, animType, x, y, row, scale);

	if (mower) {
		mEntityRegistry.AddMower(mower);
	}
	return mower.get();
}

Mower* Board::CreateMowerWithID(MowerType type, int row, float x, float y, int id)
{
	// 旧冬日花园存档可能仍含前两行小推车；加载时按当前地形契约静默丢弃。
	if (!CanHaveMowerInRow(row)) return nullptr;
	// 屋顶开发期旧存档曾把清洁车保存成 LAWN；按当前地图规范化即可无版本迁移兼容。
	if (IsRoofBackground()) {
		type = MowerType::ROOF;
		y = GetMowerTerrainY(row, x + 40.0f);
	}
	const AnimationType animType = type == MowerType::WATER
		? AnimationType::ANIM_POOL_CLEANER
		: (type == MowerType::ROOF
			? AnimationType::ANIM_ROOF_CLEANER : AnimationType::ANIM_LAWNMOWER);
	const float scale = type == MowerType::WATER ? 0.8f : 0.85f;
	auto mower = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Mower>(
		LAYER_GAME_OBJECT, this, type, animType, x, y, row, scale);

	if (mower) {
		mEntityRegistry.AddMowerWithID(mower, id);
	}
	return mower.get();
}

void Board::InitializeMowers()
{
	for (int row = 0; row < mRows; row++) {
		if (!CanHaveMowerInRow(row)) continue;
		CreateMower(IsRoofBackground()
			? MowerType::ROOF
			: (IsPoolRow(row) ? MowerType::WATER : MowerType::LAWN), row);
	}
}

/** 复制 ID 后逐一销毁其他小推车，避免 Die() 延迟回收期间修改遍历来源。 */
void Board::RemoveOtherMowersWithoutTrigger(int preservedMowerID)
{
	const std::vector<int> mowerIDs = mEntityRegistry.GetAllMowerIDs();
	for (int id : mowerIDs) {
		if (id == preservedMowerID) continue;
		Mower* mower = mEntityRegistry.GetMower(id);
		if (!mower) continue;
		SetRowLoseMower(mower->mRow);
		mower->Die();
	}
}

float Board::GetZombieCollisionY(int row) const
{
	return GetZombieCollisionY(row, CELL_INITALIZE_POS_X);
}

float Board::GetZombieCollisionY(int row, float worldX) const
{
	if (row < 0 || row >= mRows) {
		LOG_INFO("Board") << "GetZombieCollisionY: 无效的行索引: " << row;
		return -1.0f;
	}

	// 碰撞始终锚定网格逻辑行；第三大关公共地图修正仍属于整张棋盘的基线。
	const float mapAlignmentOffset = mBackGround == Background::WATER_POOL
		? kThirdAreaZombieAlignmentOffsetY
		: 0.0f;
	return GetRowCenterYAtX(row, worldX) + kZombieSpawnBaseOffsetY + mapAlignmentOffset
		+ (IsRoofBackground() ? kRoofZombieAlignmentOffsetY : 0.0f)
		+ (IsPoolBackground()
			? kPoolBackgroundZombieSpawnYOffset
			: 0.0f);
}

float Board::GetZombieSpawnY(int row) const
{
	return GetZombieSpawnY(row, CELL_INITALIZE_POS_X);
}

float Board::GetZombieSpawnY(int row, float worldX) const
{
	const float collisionY = GetZombieCollisionY(row, worldX);
	if (collisionY < 0.0f) return collisionY;

	// 水路身体需要继续保持主人校对过的下沉画面，但该偏移只影响生成/绘制基准。
	return collisionY + (IsPoolRow(row) ? kPoolRowZombieSpawnYOffset : 0.0f);
}

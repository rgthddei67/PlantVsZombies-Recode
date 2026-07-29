#pragma once

/** 黑夜天气强度；CLEAR 是所有倍率与视觉效果的单位元。 */
enum class RainIntensity {
	CLEAR,
	LIGHT,
	MEDIUM,
	HEAVY
};

/**
 * 四大关独立雾势；它与雨势正交，因此大雾可以和任意雨档、台风同时存在。
 * CLEAR 只表示没有额外“大雾天气”，不会删除关卡自身的基础迷雾。
 */
enum class FogWeatherIntensity {
	CLEAR,
	DENSE
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

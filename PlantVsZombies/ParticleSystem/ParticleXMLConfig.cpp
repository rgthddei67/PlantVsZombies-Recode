#include "ParticleXMLConfig.h"
#include "../GameRandom.h"
#include <algorithm>

float InterpolationTrack::GetValue(float normalizedTime) const {
	if (isRandomRange) {
		// 兜底：未分支到 baseScale 的消费方拿到稳定的中值，避免每帧重抽闪烁
		return (randomMin + randomMax) * 0.5f;
	}
	if (isConstant) {
		return constantValue;
	}

	if (points.empty()) {
		return 0.0f;
	}

	if (points.size() == 1) {
		return points[0].value;
	}

	// 限制时间范围
	normalizedTime = std::max(0.0f, std::min(1.0f, normalizedTime));

	// 在第一个关键帧之前 / 最后一个关键帧之后，按 PvZ 原版语义"夹住"端点值
	if (normalizedTime <= points.front().time) {
		return points.front().value;
	}
	if (normalizedTime >= points.back().time) {
		return points.back().value;
	}

	// 查找插值区间
	for (size_t i = 0; i < points.size() - 1; i++) {
		if (normalizedTime >= points[i].time && normalizedTime <= points[i + 1].time) {
			float t = (normalizedTime - points[i].time) / (points[i + 1].time - points[i].time);
			return points[i].value + t * (points[i + 1].value - points[i].value);
		}
	}

	// 不应到达
	return points.back().value;
}

float InterpolationTrack::GetValueRandomized(float normalizedTime, float randomFactor) const {
	// 单一区间："[a b]" → 用逐粒子因子在区间内取值（整生命周期保持，因子不变）
	if (isRandomRange) {
		return randomMin + randomFactor * (randomMax - randomMin);
	}
	if (isConstant) {
		return constantValue;
	}
	if (points.empty()) {
		return 0.0f;
	}
	if (points.size() == 1) {
		return points[0].Sample(randomFactor);
	}

	normalizedTime = std::max(0.0f, std::min(1.0f, normalizedTime));

	// 端点夹住，与 GetValue 语义一致，仅把"取值"换成逐粒子区间采样
	if (normalizedTime <= points.front().time) {
		return points.front().Sample(randomFactor);
	}
	if (normalizedTime >= points.back().time) {
		return points.back().Sample(randomFactor);
	}

	for (size_t i = 0; i < points.size() - 1; i++) {
		if (normalizedTime >= points[i].time && normalizedTime <= points[i + 1].time) {
			float t = (normalizedTime - points[i].time) / (points[i + 1].time - points[i].time);
			// 同一个 randomFactor 贯穿所有关键帧 → 粒子有一条稳定的扩散"轨道"，
			// 既保留"从 0 随时间扩散出去"的动画，又让每粒子落点各不相同。
			float v0 = points[i].Sample(randomFactor);
			float v1 = points[i + 1].Sample(randomFactor);
			return v0 + t * (v1 - v0);
		}
	}

	return points.back().Sample(randomFactor);
}

float InterpolationTrack::SampleConstant() const {
	if (isRandomRange) {
		return GameRandom::Range(randomMin, randomMax);
	}
	if (isConstant) {
		return constantValue;
	}
	return GetValue(0.0f);
}

float ValueRange::GetRandomValue() const {
	if (!isRange) {
		return minValue;
	}
	return GameRandom::Range(minValue, maxValue);
}

EmitterConfig::EmitterConfig() {
	// 标量/ValueRange 默认值已就近写在 ParticleXMLConfig.h 的成员声明处；
	// 这里只设置需要逐字段赋值的 InterpolationTrack / ValueRange 复合默认值。
	particleDuration = ValueRange(1.0f);

	particleAlpha.isConstant = true;
	particleAlpha.constantValue = 1.0f;

	particleScale.isConstant = true;
	particleScale.constantValue = 1.0f;

	particleStretch.isConstant = true;
	particleStretch.constantValue = 1.0f;

	particleBrightness = ValueRange(1.0f);

	particleRed.isConstant = true;
	particleRed.constantValue = 1.0f;

	particleGreen.isConstant = true;
	particleGreen.constantValue = 1.0f;

	particleBlue.isConstant = true;
	particleBlue.constantValue = 1.0f;

	systemAlpha.isConstant = true;
	systemAlpha.constantValue = 1.0f;

	launchSpeed = ValueRange(0.0f);
	particleSpinSpeed = ValueRange(0.0f);
	emitterBoxX = ValueRange(0.0f);
	emitterBoxY = ValueRange(0.0f);
	emitterRadius = ValueRange(0.0f);
}
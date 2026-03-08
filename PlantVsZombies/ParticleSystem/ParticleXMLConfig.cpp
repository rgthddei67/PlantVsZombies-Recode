#include "ParticleXMLConfig.h"
#include "../GameRandom.h"
#include <algorithm>

float InterpolationTrack::GetValue(float normalizedTime) const {
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

    // 查找插值区间
    for (size_t i = 0; i < points.size() - 1; i++) {
        if (normalizedTime >= points[i].time && normalizedTime <= points[i + 1].time) {
            // 线性插值
            float t = (normalizedTime - points[i].time) / (points[i + 1].time - points[i].time);
            return points[i].value + t * (points[i + 1].value - points[i].value);
        }
    }

    // 如果超出范围，返回最后一个值
    return points.back().value;
}

float ValueRange::GetRandomValue() const {
    if (!isRange) {
        return minValue;
    }
    return GameRandom::Range(minValue, maxValue);
}

EmitterConfig::EmitterConfig()
    : spawnMinActive(1)
    , spawnMaxLaunched(1)
    , spawnRate(0)
    , systemDuration(-1.0f)
    , emitterType(EmitterType::POINT)
    , emitterOffsetX(0.0f)
    , emitterOffsetY(0.0f)
    , randomLaunchSpin(false)
    , imageFrames(1)
    , animationRate(12.0f)
    , fullScreen(false)
    , particleGravity(100.0f) {

    // 默认值
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

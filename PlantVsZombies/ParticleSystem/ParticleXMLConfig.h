#pragma once
#ifndef __PARTICLE_XML_CONFIG_H__
#define __PARTICLE_XML_CONFIG_H__

#include <string>
#include <vector>

// 插值轨迹点
struct InterpolationPoint {
    float value;
    float time;  // 归一化时间 0-1

    InterpolationPoint() : value(0.0f), time(0.0f) {}
    InterpolationPoint(float v, float t) : value(v), time(t) {}
};

// 插值轨迹（如 "1,90 0" 表示在90%时间值为1，结束时为0）
struct InterpolationTrack {
    std::vector<InterpolationPoint> points;
    bool isConstant;
    float constantValue;

    InterpolationTrack() : isConstant(true), constantValue(1.0f) {}

    // 根据归一化时间获取插值
    float GetValue(float normalizedTime) const;
};

// 值范围（如 "[0.3 0.6]" 表示随机范围，或单个值 "1.5"）
struct ValueRange {
    float minValue;
    float maxValue;
    bool isRange;

    ValueRange() : minValue(0.0f), maxValue(0.0f), isRange(false) {}
    ValueRange(float value) : minValue(value), maxValue(value), isRange(false) {}
    ValueRange(float min, float max) : minValue(min), maxValue(max), isRange(true) {}

    // 获取随机值
    float GetRandomValue() const;
};

// 场效果类型
enum class ParticleFieldType {
    POSITION,         // 位置场（影响粒子位置）
    SHAKE,            // 抖动场（随机抖动）
    SYSTEM_POSITION,  // 系统位置场（影响发射器位置）
    FRICTION          // 摩擦力场（使粒子减速）
};

// 场效果
struct ParticleField {
    ParticleFieldType type;
    InterpolationTrack xTrack;
    InterpolationTrack yTrack;

    ParticleField() : type(ParticleFieldType::POSITION) {}
};

// 发射器形状类型
enum class EmitterType {
    POINT,   // 点发射器
    BOX,     // 盒子发射器
    CIRCLE   // 圆形发射器
};

// 发射器配置
struct EmitterConfig {
    std::string name;

    // 基础属性
    int spawnMinActive;
    int spawnMaxLaunched;
    int spawnRate;
    ValueRange particleDuration;
    float systemDuration;  // -1表示无限

    // 粒子外观
    InterpolationTrack particleAlpha;
    InterpolationTrack particleScale;
    InterpolationTrack particleStretch;
    ValueRange particleBrightness;
    InterpolationTrack particleRed;
    InterpolationTrack particleGreen;
    InterpolationTrack particleBlue;
    InterpolationTrack systemAlpha;

    // 发射器形状
    EmitterType emitterType;
    ValueRange emitterBoxX;
    ValueRange emitterBoxY;
    ValueRange emitterRadius;
    float emitterOffsetX;
    float emitterOffsetY;

    // 运动属性
    ValueRange launchSpeed;
    bool randomLaunchSpin;
    ValueRange particleSpinSpeed;
    float particleGravity;

    // 动画
    std::vector<std::string> imageKeys;  // 支持逗号分隔的多个纹理
    int imageFrames;
    float animationRate;

    // 场效果
    std::vector<ParticleField> fields;
    std::vector<ParticleField> systemFields;

    // 特殊属性
    bool fullScreen;

    EmitterConfig();
};

// 粒子特效配置（可包含多个发射器）
struct ParticleEffectConfig {
    std::string name;
    std::vector<EmitterConfig> emitters;

    ParticleEffectConfig() {}
    ParticleEffectConfig(const std::string& n) : name(n) {}
};

#endif

#pragma once
#ifndef _GAME_RANDOM_H
#define _GAME_RANDOM_H
#include <random>
#include <type_traits>
#include <memory>
#include <functional>
#include <chrono>

class GameRandom {
private:
    static inline std::mt19937_64 engine{ std::random_device{}() };

    // 预定义分布
    static inline std::uniform_real_distribution<float> floatDist{ 0.0f, 1.0f };
    static inline std::uniform_real_distribution<double> doubleDist{ 0.0, 1.0 };
    static inline std::uniform_int_distribution<int> intDist{ 0, 1 };
    static inline std::normal_distribution<float> normalDist{ 0.0f, 1.0f };

public:
    // [0.0, 1.0) 随机浮点数  
    static float Value() {
        return floatDist(engine);
    }

	// [min, max] 随机浮点数 不包含max
    static float Range(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(engine);
    }

    // [min, max] 随机整数 包含min max
    static int Range(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine);
    }

    // 随机布尔值（指定概率）
    static bool Chance(float probability = 0.5f) {
        return Value() < probability;
    }

    // 随机符号（-1 或 1）
    static int Sign() {
        return Chance() ? 1 : -1;
    }

    // 随机角度（弧度）
    static float Angle() {
        return Range(0.0f, 6.28318530718f); // 2π
    }

    // 随机方向（单位圆上的点）
    static std::pair<float, float> Direction() {
        float angle = Angle();
        return { std::cos(angle), std::sin(angle) };
    }

    // 随机颜色（RGBA）
    static std::tuple<float, float, float, float> Color(
        float alpha = 1.0f,
        bool randomAlpha = false
    ) {
        return {
            Value(), Value(), Value(),
            randomAlpha ? Value() : alpha
        };
    }

    // 高斯分布随机数
    static float Gaussian(float mean = 0.0f, float stddev = 1.0f) {
        if (mean == 0.0f && stddev == 1.0f) {
            return normalDist(engine);
        }
        std::normal_distribution<float> dist(mean, stddev);
        return dist(engine);
    }

    // 加权随机选择
    template<typename T>
    static const T& WeightedChoice(
        const std::vector<T>& items,
        const std::vector<float>& weights
    ) {
        static_assert(std::is_arithmetic_v<float>, "Weights must be numeric");

        float totalWeight = 0.0f;
        for (float w : weights) totalWeight += w;

        float random = Range(0.0f, totalWeight);
        float cumulative = 0.0f;

        for (size_t i = 0; i < items.size(); ++i) {
            cumulative += weights[i];
            if (random <= cumulative) {
                return items[i];
            }
        }

        return items.back(); // 理论上不会到这里
    }

    // 洗牌（Fisher-Yates算法）
    template<typename T>
    static void Shuffle(std::vector<T>& items) {
        for (int i = items.size() - 1; i > 0; --i) {
            int j = Range(0, i);
            std::swap(items[i], items[j]);
        }
    }

    // 设置随机种子
    static void SetSeed(uint64_t seed) {
        engine.seed(seed);
    }

    // 获取当前种子
    static uint64_t GetSeed() {
        return engine.default_seed;
    }

    // 随机种子（基于时间）
    static void RandomizeSeed() {
        engine.seed(std::chrono::high_resolution_clock::now()
            .time_since_epoch().count());
    }
};

#endif
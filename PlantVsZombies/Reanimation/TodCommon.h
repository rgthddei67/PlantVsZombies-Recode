#pragma once

#ifndef _TODCOMMON_H
#define _TODCOMMON_H
#include "../AllCppInclude.h"
#include <cmath>
#include <algorithm>
#include "../Board&Game/Definit.h"

// 数学常量
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

// 数学工具函数
inline bool FloatApproxEqual(float a, float b, float epsilon = 0.001f) {
    return fabs(a - b) < epsilon;
}

inline float FloatLerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline int FloatRoundToInt(float f) {
    return static_cast<int>(f + 0.5f);
}

inline int ClampInt(int value, int minVal, int maxVal) {
    return std::max(minVal, std::min(value, maxVal));
}

inline float m_sqrtf(float x) {
    return sqrt(x);
}

// 颜色工具
struct Color {
    Uint8 r, g, b, a;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {
    }

    static const Color White;
    static const Color Black;
};

inline Color ColorsMultiply(const Color& color1, const Color& color2) {
    return Color(
        (color1.r * color2.r) / 255,
        (color1.g * color2.g) / 255,
        (color1.b * color2.b) / 255,
        (color1.a * color2.a) / 255
    );
}

inline Uint8 ColorComponentMultiply(Uint8 a, Uint8 b) {
    return (a * b) / 255;
}

#endif
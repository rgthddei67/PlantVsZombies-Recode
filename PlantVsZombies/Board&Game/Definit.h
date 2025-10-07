#pragma once
#ifndef _DEFINIT_H
#define _DEFINIT_H
#include <SDL.h>
#include <math.h>

struct Vector
{
    float x;
    float y;
    Vector()
    {
        x = 0, y = 0;
    }
    Vector(float x, float y)
    {
        this->x = x;
        this->y = y;
    }
    operator SDL_FPoint() const 
    { 
        return { x, y }; 
    }
    SDL_FPoint ToSDL_FPoint() const {
        return { x, y };
    }
    bool operator==(const Vector& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const Vector& other) const {
        return !(*this == other);
    }
    bool operator<(const Vector& other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
    bool operator>(const Vector& other) const {
        if (x != other.x) return x > other.x;
        return y > other.y;
    }
    Vector operator+(const Vector& other) const {
        return Vector(x + other.x, y + other.y);
    }
    Vector operator-(const Vector& other) const {
        return Vector(x - other.x, y - other.y);
    }
    Vector operator*(float scalar) const {
        return Vector(x * scalar, y * scalar);
    }
    Vector operator/(float scalar) const {
        return Vector(x / scalar, y / scalar);
    }
    Vector& operator+=(const Vector& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    Vector& operator-=(const Vector& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    Vector& operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }
    Vector& operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }
    // 计算向量长度（模）
    float magnitude() const {
        return sqrtf(x * x + y * y);
    }
    // 计算向量长度的平方（避免开方运算，用于比较长度时更高效）
    float sqrMagnitude() const {
        return x * x + y * y;
    }
    // 归一化向量（使其长度为1）
    Vector normalized() const {
        float mag = magnitude();
        if (mag > 0) {
            return Vector(x / mag, y / mag);
        }
        return Vector(0, 0);
    }
    // 点积（内积）
    float dot(const Vector& other) const {
        return x * other.x + y * other.y;
    }
    // 计算两点之间的距离
    static float distance(const Vector& a, const Vector& b) {
        return (a - b).magnitude();
    }
    // 计算两点之间距离的平方
    static float sqrDistance(const Vector& a, const Vector& b) {
        return (a - b).sqrMagnitude();
    }
    // 线性插值
    static Vector lerp(const Vector& a, const Vector& b, float t) {
        t = (t < 0) ? 0 : (t > 1) ? 1 : t; // 限制t在0-1之间
        return a + (b - a) * t;
    }

    static Vector zero() { return Vector(0, 0); }
    static Vector one() { return Vector(1, 1); }
    static Vector up() { return Vector(0, -1); } // 屏幕坐标系中Y轴向下为正
    static Vector down() { return Vector(0, 1); }
    static Vector left() { return Vector(-1, 0); }
    static Vector right() { return Vector(1, 0); }
};

// 矩阵类
struct SexyTransform2D 
{
    float m00, m01, m02;
    float m10, m11, m12;
    float m20, m21, m22;

    SexyTransform2D() {
        LoadIdentity();
    }

    void LoadIdentity() {
        m00 = 1.0f; m01 = 0.0f; m02 = 0.0f;
        m10 = 0.0f; m11 = 1.0f; m12 = 0.0f;
        m20 = 0.0f; m21 = 0.0f; m22 = 1.0f;
    }
};

// SexyVector2 类似于 Vector，但用于矩阵运算
struct SexyVector2 
{
    float x, y;
    SexyVector2() : x(0), y(0) {}
    SexyVector2(float x, float y) : x(x), y(y) {}
};

// 矩阵乘法函数
inline void SexyMatrix3Multiply(SexyTransform2D& result, const SexyTransform2D& mat1, const SexyTransform2D& mat2) {
    result.m00 = mat1.m00 * mat2.m00 + mat1.m01 * mat2.m10 + mat1.m02 * mat2.m20;
    result.m01 = mat1.m00 * mat2.m01 + mat1.m01 * mat2.m11 + mat1.m02 * mat2.m21;
    result.m02 = mat1.m00 * mat2.m02 + mat1.m01 * mat2.m12 + mat1.m02 * mat2.m22;

    result.m10 = mat1.m10 * mat2.m00 + mat1.m11 * mat2.m10 + mat1.m12 * mat2.m20;
    result.m11 = mat1.m10 * mat2.m01 + mat1.m11 * mat2.m11 + mat1.m12 * mat2.m21;
    result.m12 = mat1.m10 * mat2.m02 + mat1.m11 * mat2.m12 + mat1.m12 * mat2.m22;

    result.m20 = mat1.m20 * mat2.m00 + mat1.m21 * mat2.m10 + mat1.m22 * mat2.m20;
    result.m21 = mat1.m20 * mat2.m01 + mat1.m21 * mat2.m11 + mat1.m22 * mat2.m21;
    result.m22 = mat1.m20 * mat2.m02 + mat1.m21 * mat2.m12 + mat1.m22 * mat2.m22;
}

inline void SexyMatrix3Translation(SexyTransform2D& mat, float x, float y) {
    mat.m02 += x;
    mat.m12 += y;
}

#endif
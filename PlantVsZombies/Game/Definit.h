#pragma once
#ifndef _DEFINIT_H
#define _DEFINIT_H
#include <SDL2/SDL.h>
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

    // 平移
    void Translate(float x, float y) {
        m02 += x;
        m12 += y;
    }

    // 缩放
    void Scale(float sx, float sy) {
        m00 *= sx; m01 *= sx; m02 *= sx;
        m10 *= sy; m11 *= sy; m12 *= sy;
    }

    // 旋转（角度制）
    void Rotate(float angle) {
        float rad = angle * 3.14159265f / 180.0f;
        float cosA = cosf(rad);
        float sinA = sinf(rad);

        SexyTransform2D rot;
        rot.m00 = cosA;  rot.m01 = -sinA; rot.m02 = 0;
        rot.m10 = sinA;  rot.m11 = cosA;  rot.m12 = 0;
        rot.m20 = 0;     rot.m21 = 0;     rot.m22 = 1;

        *this = Multiply(*this, rot);
    }

    // 斜切
    void Shear(float kx, float ky) {
        SexyTransform2D shear;
        shear.m00 = 1;   shear.m01 = kx;  shear.m02 = 0;
        shear.m10 = ky;  shear.m11 = 1;   shear.m12 = 0;
        shear.m20 = 0;   shear.m21 = 0;   shear.m22 = 1;

        *this = Multiply(*this, shear);
    }

    // 矩阵乘法
    static SexyTransform2D Multiply(const SexyTransform2D& a, const SexyTransform2D& b) {
        SexyTransform2D result;

        result.m00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20;
        result.m01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21;
        result.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22;

        result.m10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20;
        result.m11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21;
        result.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22;

        result.m20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20;
        result.m21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21;
        result.m22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22;

        return result;
    }

    // 变换点
    Vector TransformPoint(const Vector& point) const {
        float x = m00 * point.x + m01 * point.y + m02;
        float y = m10 * point.x + m11 * point.y + m12;
        return Vector(x, y);
    }
};

#endif
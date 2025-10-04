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
    // �����������ȣ�ģ��
    float magnitude() const {
        return sqrtf(x * x + y * y);
    }
    // �����������ȵ�ƽ�������⿪�����㣬���ڱȽϳ���ʱ����Ч��
    float sqrMagnitude() const {
        return x * x + y * y;
    }
    // ��һ��������ʹ�䳤��Ϊ1��
    Vector normalized() const {
        float mag = magnitude();
        if (mag > 0) {
            return Vector(x / mag, y / mag);
        }
        return Vector(0, 0);
    }
    // ������ڻ���
    float dot(const Vector& other) const {
        return x * other.x + y * other.y;
    }
    // ��������֮��ľ���
    static float distance(const Vector& a, const Vector& b) {
        return (a - b).magnitude();
    }
    // ��������֮������ƽ��
    static float sqrDistance(const Vector& a, const Vector& b) {
        return (a - b).sqrMagnitude();
    }
    // ���Բ�ֵ
    static Vector lerp(const Vector& a, const Vector& b, float t) {
        t = (t < 0) ? 0 : (t > 1) ? 1 : t; // ����t��0-1֮��
        return a + (b - a) * t;
    }

    static Vector zero() { return Vector(0, 0); }
    static Vector one() { return Vector(1, 1); }
    static Vector up() { return Vector(0, -1); } // ��Ļ����ϵ��Y������Ϊ��
    static Vector down() { return Vector(0, 1); }
    static Vector left() { return Vector(-1, 0); }
    static Vector right() { return Vector(1, 0); }
};

// ������
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

// SexyVector2 ������ Vector�������ھ�������
struct SexyVector2 
{
    float x, y;
    SexyVector2() : x(0), y(0) {}
    SexyVector2(float x, float y) : x(x), y(y) {}
};

// ����˷�����
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
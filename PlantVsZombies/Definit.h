#pragma once
#ifndef _DEFINIT_H
#define _DEFINIT_H
#include <SDL.h>

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

#endif
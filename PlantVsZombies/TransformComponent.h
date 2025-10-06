#pragma once
#ifndef _TRANSFORM_COMPONENT_H
#define _TRANSFORM_COMPONENT_H

#include "Component.h"

class TransformComponent : public Component {
public:
    Vector position = Vector::zero();
    Vector scale = Vector::one();
    float rotation = 0.0f;  // 旋转的弧度

    TransformComponent() = default;
    TransformComponent(const Vector& pos) : position(pos) {}
    TransformComponent(float x, float y) : position(x, y) {}

    // 移动x,y Vector版本
    void Translate(const Vector& translation) {
        position += translation;
    }

    // 移动x,y float版本
    void Translate(float x, float y) {
        position.x += x;
        position.y += y;
    }

    // 旋转angle°
    void Rotate(float angle) {
        rotation += angle;
    }

    // 设置angle°
    void SetRotation(float angle) {
        rotation = angle;
    }

    // 缩放
    void Scale(const Vector& scaling) {
        scale.x *= scaling.x;
        scale.y *= scaling.y;
    }

    // 设置缩放
    void SetScale(const Vector& scaling) {
        scale = scaling;
    }

    // 获取前面向量
    Vector GetForward() const {
        return Vector(cosf(rotation), sinf(rotation));
    }

    // 获取右边向量
    Vector GetRight() const {
        return Vector(-sinf(rotation), cosf(rotation));
    }

	// 设置位置 Vector版本
    void SetPosition(const Vector& newPos) {
        position = newPos;
    }

    // 设置位置 float版本
    void SetPosition(float x, float y) {
        position.x = x;
        position.y = y;
    }

    // 获取世界位置（如果有父对象需要计算，这里简化）
    Vector GetWorldPosition() const {
        return position;
    }
};

#endif
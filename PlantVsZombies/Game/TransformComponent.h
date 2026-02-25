#pragma once
#ifndef _TRANSFORM_COMPONENT_H
#define _TRANSFORM_COMPONENT_H

#include "Component.h"
#include "GameObject.h"
#include "../GameApp.h"

class TransformComponent : public Component {
private:
	Vector position = Vector::zero();
	float scale = 1.0f;
	float rotation = 0.0f;  // 旋转的弧度

public:
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
	void Scale(float& scaling) {
		scale *= scaling;
	}

	// 设置缩放
	void SetScale(float& scaling) {
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

	// 获取世界位置
	Vector GetPosition() const {
		return position;
	}

	float GetScale() const {
		return scale;
	}

	float GetRotation() const {
		return rotation;
	}

};

#endif
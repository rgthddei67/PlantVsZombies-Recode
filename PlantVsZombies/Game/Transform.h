#pragma once

#include "Definit.h"
#include <cmath>

/**
 * @brief GameObject 可选拥有的非多态空间值，统一保存世界位置、缩放与旋转。
 */
class Transform {
public:
	Transform() = default;
	explicit Transform(const Vector& position) : mPosition(position) {}
	Transform(float x, float y) : mPosition(x, y) {}

	void Translate(const Vector& translation) { mPosition += translation; }
	void Translate(float x, float y) {
		mPosition.x += x;
		mPosition.y += y;
	}

	void Rotate(float angle) { mRotation += angle; }
	void SetRotation(float angle) { mRotation = angle; }
	void Scale(float scaling) { mScale *= scaling; }
	void SetScale(float scaling) { mScale = scaling; }

	Vector GetForward() const {
		return Vector(std::cos(mRotation), std::sin(mRotation));
	}
	Vector GetRight() const {
		return Vector(-std::sin(mRotation), std::cos(mRotation));
	}

	void SetPosition(const Vector& position) { mPosition = position; }
	void SetPosition(float x, float y) {
		mPosition.x = x;
		mPosition.y = y;
	}
	Vector GetPosition() const { return mPosition; }
	float GetScale() const { return mScale; }
	float GetRotation() const { return mRotation; }

	/** @brief 为对象池复用恢复完整中性变换，而不是只覆盖位置。 */
	void Reset(const Vector& position = Vector::zero()) {
		mPosition = position;
		mScale = 1.0f;
		mRotation = 0.0f;
	}

private:
	Vector mPosition = Vector::zero();
	float mScale = 1.0f;
	float mRotation = 0.0f; // 世界旋转弧度
};

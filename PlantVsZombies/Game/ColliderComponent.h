#pragma once
#ifndef _COLLIDER_COMPONENT_H
#define _COLLIDER_COMPONENT_H

#include "Component.h"
#include "Definit.h"
#include <SDL2/SDL.h>
#include <functional>
#include <cstdint>

class TransformComponent;

// 碰撞形状类型
enum class ColliderType {
	BOX,
	CIRCLE
};

namespace CollisionLayer {
	constexpr uint16_t NONE   = 0;
	constexpr uint16_t PLANT  = 1 << 0;
	constexpr uint16_t ZOMBIE = 1 << 1;
	constexpr uint16_t BULLET = 1 << 2;
	constexpr uint16_t MOWER  = 1 << 3;
	constexpr uint16_t COIN   = 1 << 4;
	constexpr uint16_t ALL    = 0xFFFF;
}

class ColliderComponent : public Component {
public:
	Vector offset = Vector::zero();    // 相对于游戏对象的偏移
	Vector size = Vector(50, 40);        // 尺寸（矩形为宽高，圆形为直径）
	ColliderType colliderType = ColliderType::BOX;
	bool isTrigger = false;            // 是否是触发器
	bool isStatic = false;             // 是否是静态碰撞体

	uint16_t layerMask     = CollisionLayer::ALL;
	uint16_t collisionMask = CollisionLayer::ALL;
	uint32_t colliderID    = 0;
	bool     mRegistered   = false;

	// 碰撞的事件（回调函数） —— 裸指针 other，回调阶段保证对象活
	std::function<void(ColliderComponent*)> onTriggerEnter;
	std::function<void(ColliderComponent*)> onTriggerStay;
	std::function<void(ColliderComponent*)> onTriggerExit;
	std::function<void(ColliderComponent*)> onCollisionEnter;
	std::function<void(ColliderComponent*)> onCollisionExit;

	SDL_Color debugColor = { 255, 0, 0, 255 }; // 调试颜色（红色）

	// 帧缓存：由 CollisionSystem::Update 在帧首一次性写入，CheckCollision 直接读取，
	// 避免每次检测都走 GetComponent<TransformComponent>() 的 unordered_map 查找。
	SDL_FRect cachedBounds{ 0, 0, 0, 0 };
	Vector    cachedWorldPos;

	ColliderComponent(const Vector& size, const Vector& offset = Vector(0, 0), ColliderType type = ColliderType::BOX)
		: size(size), offset(offset), colliderType(type) {
	}

	// 获取世界空间位置
	Vector GetWorldPosition() const;

	// 获取边界框
	SDL_FRect GetBoundingBox() const;

	// 检查点是否在碰撞器内(点在世界空间坐标) Vector参数
	bool ContainsPoint(const Vector& point) const;

	// 检查点是否在碰撞器内(点在世界空间坐标) float参数
	bool ContainsPoint(float x, float y) const {
		return ContainsPoint(Vector(x, y));
	}

	void Draw(Graphics* g) override;

	// 绘制矩形碰撞框
	void DrawBoxCollider(Graphics* g, const SDL_FRect& rect);

	// 绘制圆形碰撞框
	void DrawCircleCollider(Graphics* g, const Vector& center, float radius);

private:
	TransformComponent* GetTransform() const;
};

#endif

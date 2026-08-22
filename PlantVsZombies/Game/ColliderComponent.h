#pragma once
#ifndef _COLLIDER_COMPONENT_H
#define _COLLIDER_COMPONENT_H

#include "Definit.h"
#include <SDL2/SDL.h>
#include <functional>
#include <cstdint>
#include <memory>

class Transform;
class GameObject;
class Graphics;
class CollisionSystem;

// 碰撞形状类型
enum class ColliderType {
	BOX,
	CIRCLE
};

namespace CollisionLayer {
	constexpr uint16_t NONE = 0;
	constexpr uint16_t PLANT = 1 << 0;
	constexpr uint16_t ZOMBIE = 1 << 1;
	constexpr uint16_t BULLET = 1 << 2;
	constexpr uint16_t MOWER = 1 << 3;
	constexpr uint16_t COIN = 1 << 4;
	constexpr uint16_t CHARMED = 1 << 5;   // 魅惑僵尸专用层：落入碰撞 seeker 桶（mRowOthers），见 CollisionSystem 拆分说明
	constexpr uint16_t ALL = 0xFFFF;
}

/**
 * @brief GameObject 显式拥有的可选碰撞附件。
 * @details 只能由 GameObject::CreateCollider 创建，所有权与注册生命周期都由宿主显式管理。
 */
class ColliderComponent {
public:
	using CollisionCallback = std::function<void(ColliderComponent*)>;

	Vector offset = Vector::zero();    // 相对于游戏对象的偏移
	Vector size = Vector(50, 40);        // 尺寸（矩形为宽高，圆形为直径）
	ColliderType colliderType = ColliderType::BOX;
	bool isTrigger = false;            // 是否是触发器
	bool isStatic = false;             // 是否是静态碰撞体

	uint16_t layerMask = CollisionLayer::ALL;
	uint16_t collisionMask = CollisionLayer::ALL;
	bool mEnabled = true;

	SDL_Color debugColor = { 255, 0, 0, 255 }; // 调试颜色（红色）

	// 帧缓存：由 CollisionSystem::Update 在帧首一次性写入，CheckCollision 直接读取，
	// 避免每次窄相检测都重复组合宿主 Transform 与碰撞体局部几何。
	SDL_FRect cachedBounds{ 0, 0, 0, 0 };
	Vector    cachedWorldPos;

	GameObject* GetGameObject() const { return mGameObject; }

	/** 设置触发器进入回调；空回调会释放不再使用的冷状态。 */
	void SetTriggerEnterCallback(CollisionCallback callback);
	/** 设置触发器持续回调；空回调会释放不再使用的冷状态。 */
	void SetTriggerStayCallback(CollisionCallback callback);
	/** 设置触发器离开回调；空回调会释放不再使用的冷状态。 */
	void SetTriggerExitCallback(CollisionCallback callback);
	/** 设置实体碰撞进入回调；空回调会释放不再使用的冷状态。 */
	void SetCollisionEnterCallback(CollisionCallback callback);
	/** 设置实体碰撞离开回调；空回调会释放不再使用的冷状态。 */
	void SetCollisionExitCallback(CollisionCallback callback);

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

	void Draw(Graphics* g);

	// 绘制矩形碰撞框
	void DrawBoxCollider(Graphics* g, const SDL_FRect& rect);

	// 绘制圆形碰撞框
	void DrawCircleCollider(Graphics* g, const Vector& center, float radius);

private:
	friend class GameObject;
	friend class CollisionSystem;

	GameObject* mGameObject = nullptr; // 非拥有；生命周期严格短于宿主
	uint32_t colliderID = 0;
	bool mRegistered = false;
	struct TriggerCallbacks {
		CollisionCallback enter;
		CollisionCallback stay;
		CollisionCallback exit;
	};
	struct CollisionCallbacks {
		CollisionCallback enter;
		CollisionCallback exit;
	};
	std::unique_ptr<TriggerCallbacks> mTriggerCallbacks;
	std::unique_ptr<CollisionCallbacks> mCollisionCallbacks;

	ColliderComponent(GameObject* owner, const Vector& size,
		const Vector& offset, ColliderType type)
		: offset(offset), size(size), colliderType(type), mGameObject(owner) {}

	Transform* GetTransform() const;
	bool HasTriggerEnterCallback() const;
	bool HasTriggerStayCallback() const;
	bool HasTriggerExitCallback() const;
	bool HasCollisionEnterCallback() const;
	bool HasCollisionExitCallback() const;
	void InvokeTriggerEnter(ColliderComponent* other) const;
	void InvokeTriggerStay(ColliderComponent* other) const;
	void InvokeTriggerExit(ColliderComponent* other) const;
	void InvokeCollisionEnter(ColliderComponent* other) const;
	void InvokeCollisionExit(ColliderComponent* other) const;
	void ReleaseEmptyCallbackState();
};

#endif

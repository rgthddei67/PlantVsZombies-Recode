#pragma once
#ifndef _COLLIDER_COMPONENT_H
#define _COLLIDER_COMPONENT_H

#include "Component.h"
#include "TransformComponent.h"
#include "Definit.h"
#include <SDL.h>
#include <functional>
#include <memory>

// 碰撞形状类型
enum class ColliderType {
    BOX,
    CIRCLE
};

class ColliderComponent : public Component {
public:
    Vector offset = Vector::zero();    // 相对于游戏对象的偏移
    Vector size = Vector(50, 40);        // 尺寸（矩形为宽高，圆形为直径）
    ColliderType colliderType = ColliderType::BOX;
    bool isTrigger = false;            // 是否是触发器
    bool isStatic = false;             // 是否是静态碰撞体

    // 碰撞的事件（回调函数）
    std::function<void(std::shared_ptr<ColliderComponent>)> onTriggerEnter;
    std::function<void(std::shared_ptr<ColliderComponent>)> onTriggerStay;
    std::function<void(std::shared_ptr<ColliderComponent>)> onTriggerExit;
    std::function<void(std::shared_ptr<ColliderComponent>)> onCollisionEnter;
    std::function<void(std::shared_ptr<ColliderComponent>)> onCollisionExit;

    ColliderComponent() = default;

    ColliderComponent(const Vector& size, ColliderType type = ColliderType::BOX)
        : size(size), colliderType(type) {
    }

    ColliderComponent(float width, float height)
        : size(width, height), colliderType(ColliderType::BOX) {
    }

    // 获取世界坐标位置
    Vector GetWorldPosition() const {
        if (auto transform = GetTransform()) {
            return transform->position + offset;
        }
        return offset;
    }

    // 获取边界框
    SDL_FRect GetBoundingBox() const {
        Vector worldPos = GetWorldPosition();

        if (colliderType == ColliderType::CIRCLE) {
            float radius = size.x * 0.5f;
            return {
                worldPos.x - radius,
                worldPos.y - radius,
                size.x,
                size.y
            };
        }
        else {
            return {
                worldPos.x - size.x * 0.5f,
                worldPos.y - size.y * 0.5f,
                size.x,
                size.y
            };
        }
    }

	// 检查点是否在碰撞体内(点在世界坐标系中) Vector版本
    bool ContainsPoint(const Vector& point) const {
        Vector worldPos = GetWorldPosition();

        switch (colliderType) {
        case ColliderType::BOX: {
            SDL_FRect rect = GetBoundingBox();
            return (point.x >= rect.x && point.x <= rect.x + rect.w &&
                point.y >= rect.y && point.y <= rect.y + rect.h);
        }

        case ColliderType::CIRCLE: {
            float radius = size.x * 0.5f;
            float distance = Vector::distance(worldPos, point);
            return distance <= radius;
        }
        }
        return false;
    }
	// 检查点是否在碰撞体内(点在世界坐标系中) float版本
    bool ContainsPoint(float x, float y) const {
        return ContainsPoint(Vector(x, y));
	}

private:
    std::shared_ptr<TransformComponent> GetTransform() const {
        if (auto gameObj = GetGameObject()) 
        {  
            return gameObj->GetComponent<TransformComponent>();
        }
        return nullptr;
    }
};

#endif
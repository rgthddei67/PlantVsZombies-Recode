#pragma once
#ifndef _COLLIDER_COMPONENT_H
#define _COLLIDER_COMPONENT_H

#include "Component.h"
#include "Definit.h"
#include <SDL2/SDL.h>
#include <functional>
#include <memory>

class TransformComponent;

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

    SDL_Color debugColor = { 255, 0, 0, 255 }; // 调试颜色（红色）

    ColliderComponent() = default;

    ColliderComponent(const Vector& size, ColliderType type = ColliderType::BOX)
        : size(size), colliderType(type) {
    }

    ColliderComponent(float width, float height)
        : size(width, height), colliderType(ColliderType::BOX) {
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

    void Draw(SDL_Renderer* renderer) override;

    // 绘制矩形碰撞框
    void DrawBoxCollider(SDL_Renderer* renderer, const SDL_FRect& rect);

    // 绘制圆形碰撞框
    void DrawCircleCollider(SDL_Renderer* renderer, const Vector& center, float radius);

private:
    std::shared_ptr<TransformComponent> GetTransform() const;
};

#endif
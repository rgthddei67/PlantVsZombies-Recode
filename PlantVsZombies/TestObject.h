#pragma once
#include <iostream>
#include "SDL.h"
#include "./Game/Component.h"
#include "./Game/TransformComponent.h"
#include "./Game/ColliderComponent.h"

class TestObject : public GameObject {
private:
    std::string objectName;
    SDL_Color debugColor;

public:
    TestObject(const Vector& position, const Vector& size, ColliderType colliderType,
        const std::string& name, const SDL_Color& color = { 255, 255, 255, 255 })
        : objectName(name), debugColor(color) {

        SetName(name);
        SetTag("TestObject");

        // 添加变换组件
        auto transform = AddComponent<TransformComponent>();
        transform->position = position;

        // 添加碰撞组件
        auto collider = AddComponent<ColliderComponent>();
        collider->size = size;
        collider->colliderType = colliderType;
        collider->isTrigger = false;

        // 设置碰撞回调
        collider->onCollisionEnter = [this](std::shared_ptr<ColliderComponent> other) {
            this->OnCollisionEnter(other);
            };

        collider->onCollisionExit = [this](std::shared_ptr<ColliderComponent> other) {
            this->OnCollisionExit(other);
            };
    }

    void Update() override {
        GameObject::Update();

        TestMovement();
    }

    // 获取调试信息
    const std::string& GetDebugName() const { return objectName; }
    const SDL_Color& GetDebugColor() const { return debugColor; }

private:
    void OnCollisionEnter(std::shared_ptr<ColliderComponent> other) {
        std::cout << "[碰撞开始] " << objectName << " 与 "
            << other->GetGameObject()->GetName() << " 发生碰撞!" << std::endl;

        // 碰撞时改变颜色或其他视觉效果
        debugColor = { 255, 0, 0, 255 }; // 变为红色
    }

    void OnCollisionExit(std::shared_ptr<ColliderComponent> other) {
        std::cout << "[碰撞结束] " << objectName << " 与 "
            << other->GetGameObject()->GetName() << " 分离!" << std::endl;

        // 恢复原色
        debugColor = { 255, 255, 255, 255 };
    }

    void TestMovement() {
        // 简单的测试移动逻辑
        float moveSpeed = 4 + rand() % 30;
        auto transform = GetComponent<TransformComponent>();
        if (transform) {
            transform->position.x += moveSpeed * 0.2f;

            // 边界检查
            if (transform->position.x > 800) {
                transform->position.x = 0;
            }
        }
    }
};

class DebugRenderer {
public:
    static void RenderColliders(SDL_Renderer* renderer) {
        const auto& gameObjects = GameObjectManager::GetInstance().FindGameObjectsWithTag("动态方块");
        for (auto& obj : gameObjects) {
            auto transform = obj->GetComponent<TransformComponent>();
            auto collider = obj->GetComponent<ColliderComponent>();

            if (transform && collider) {
                auto testObj = std::dynamic_pointer_cast<TestObject>(obj);
                if (testObj) {
                    RenderCollider(renderer, transform, collider, testObj->GetDebugColor());
                }
                else {
                    RenderCollider(renderer, transform, collider, { 255, 255, 255, 255 });
                }
            }
        }
    }

private:
    static void RenderCollider(SDL_Renderer* renderer,
        std::shared_ptr<TransformComponent> transform,
        std::shared_ptr<ColliderComponent> collider,
        const SDL_Color& color) {

        SDL_FRect bounds = collider->GetBoundingBox();

        // 使用传入的颜色
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

        if (collider->colliderType == ColliderType::BOX) {
            SDL_Rect rect = {
                static_cast<int>(bounds.x),
                static_cast<int>(bounds.y),
                static_cast<int>(bounds.w),
                static_cast<int>(bounds.h)
            };
            SDL_RenderDrawRect(renderer, &rect);
        }
        else if (collider->colliderType == ColliderType::CIRCLE) {
            DrawCircle(renderer,
                static_cast<int>(transform->position.x),
                static_cast<int>(transform->position.y),
                static_cast<int>(collider->size.x * 0.5f));
        }
    }

    static void DrawCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius) {
        // 简单的圆形绘制
        for (int angle = 0; angle < 360; angle += 10) {
            float rad = angle * 3.14159f / 180.0f;
            int x1 = centerX + static_cast<int>(radius * cosf(rad));
            int y1 = centerY + static_cast<int>(radius * sinf(rad));
            int x2 = centerX + static_cast<int>(radius * cosf((angle + 10) * 3.14159f / 180.0f));
            int y2 = centerY + static_cast<int>(radius * sinf((angle + 10) * 3.14159f / 180.0f));

            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }
};
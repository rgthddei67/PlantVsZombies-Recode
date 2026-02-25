#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "Component.h"
#include "../GameApp.h"
#include <memory>

Vector ColliderComponent::GetWorldPosition() const {
    if (auto transform = GetTransform()) {
        return transform->GetPosition() + offset;
    }
    return offset;
} 

SDL_FRect ColliderComponent::GetBoundingBox() const {
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
            worldPos.x,      
            worldPos.y,          
            size.x,               // 宽度
            size.y                // 高度
        };
    }
}

bool ColliderComponent::ContainsPoint(const Vector& point) const {
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

std::shared_ptr<TransformComponent> ColliderComponent::GetTransform() const {
    if (auto gameObj = GetGameObject()) {
        return gameObj->GetComponent<TransformComponent>();
    }
    return nullptr;
}

void ColliderComponent::Draw(Graphics* g) {
    if (!GameAPP::mDebugMode || !GameAPP::mShowColliders || !mEnabled) return;

    Vector worldPos = GetWorldPosition();

    switch (colliderType) {
    case ColliderType::BOX: {
        SDL_FRect rect = GetBoundingBox();
        DrawBoxCollider(g, rect);
        break;
    }
    case ColliderType::CIRCLE: {
        float radius = size.x * 0.5f;
        DrawCircleCollider(g, worldPos, radius);
        break;
    }
    }
}

// 绘制矩形碰撞框
void ColliderComponent::DrawBoxCollider(Graphics* g, const SDL_FRect& rect) {
    // 将调试颜色转换为 glm::vec4 (0~1)
    glm::vec4 color(debugColor.r / 255.0f,
        debugColor.g / 255.0f,
        debugColor.b / 255.0f,
        debugColor.a / 255.0f);

    // 为清晰显示，稍微扩大矩形
    float x = rect.x - 1.0f;
    float y = rect.y - 1.0f;
    float w = rect.w + 2.0f;
    float h = rect.h + 2.0f;

    if (isTrigger) {
        // 绘制虚线边框
        float dashLength = 5.0f;
        float gapLength = 3.0f;

        // 上边虚线
        for (float cx = x; cx < x + w; cx += dashLength + gapLength) {
            float endX = std::min(cx + dashLength, x + w);
            g->DrawLine(cx, y, endX, y, color);
        }
        // 下边虚线
        for (float cx = x; cx < x + w; cx += dashLength + gapLength) {
            float endX = std::min(cx + dashLength, x + w);
            g->DrawLine(cx, y + h, endX, y + h, color);
        }
        // 左边虚线
        for (float cy = y; cy < y + h; cy += dashLength + gapLength) {
            float endY = std::min(cy + dashLength, y + h);
            g->DrawLine(x, cy, x, endY, color);
        }
        // 右边虚线
        for (float cy = y; cy < y + h; cy += dashLength + gapLength) {
            float endY = std::min(cy + dashLength, y + h);
            g->DrawLine(x + w, cy, x + w, endY, color);
        }
    }
    else {
        // 绘制实线边框
        g->DrawRect(x, y, w, h, color);
    }
}

// 绘制圆形碰撞框
void ColliderComponent::DrawCircleCollider(Graphics* g, const Vector& center, float radius) {
    glm::vec4 color(debugColor.r / 255.0f,
        debugColor.g / 255.0f,
        debugColor.b / 255.0f,
        debugColor.a / 255.0f);

    // 绘制空心圆（使用32个线段）
    g->DrawCircle(center.x, center.y, radius, color);

    if (isTrigger) {
        // 绘制十字线表示触发器
        g->DrawLine(center.x - radius, center.y, center.x + radius, center.y, color);
        g->DrawLine(center.x, center.y - radius, center.x, center.y + radius, color);
    }
}
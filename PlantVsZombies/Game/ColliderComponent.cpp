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

void ColliderComponent::Draw(SDL_Renderer* renderer) {
    if (!GameAPP::mDebugMode || !GameAPP::mShowColliders || !mEnabled) return;

    Vector worldPos = GetWorldPosition();

    switch (colliderType) {
    case ColliderType::BOX: {
        SDL_FRect rect = GetBoundingBox();
        DrawBoxCollider(renderer, rect);
        break;
    }
    case ColliderType::CIRCLE: {
        float radius = size.x * 0.5f;
        DrawCircleCollider(renderer, worldPos, radius);
        break;
    }
    }
}

// 绘制矩形碰撞框
void ColliderComponent::DrawBoxCollider(SDL_Renderer* renderer, const SDL_FRect& rect) {
    // 保存原始绘制颜色
    SDL_Color originalColor;
    SDL_GetRenderDrawColor(renderer, &originalColor.r, &originalColor.g, &originalColor.b, &originalColor.a);

    // 设置调试颜色
    SDL_SetRenderDrawColor(renderer, debugColor.r, debugColor.g, debugColor.b, debugColor.a);

    // 绘制矩形边框
    SDL_FRect outline = {
        rect.x - 1, rect.y - 1,  // 稍微偏移以更清晰显示
        rect.w + 2, rect.h + 2
    };

    // 绘制四条边
    SDL_RenderDrawLineF(renderer, outline.x, outline.y, outline.x + outline.w, outline.y); // 上边
    SDL_RenderDrawLineF(renderer, outline.x, outline.y + outline.h, outline.x + outline.w, outline.y + outline.h); // 下边
    SDL_RenderDrawLineF(renderer, outline.x, outline.y, outline.x, outline.y + outline.h); // 左边
    SDL_RenderDrawLineF(renderer, outline.x + outline.w, outline.y, outline.x + outline.w, outline.y + outline.h); // 右边

    // 如果是触发器，绘制虚线边框
    if (isTrigger) {
        // 绘制虚线（简单的间隔线）
        float dashLength = 5.0f;
        float gapLength = 3.0f;

        // 上边虚线
        for (float x = outline.x; x < outline.x + outline.w; x += dashLength + gapLength) {
            SDL_RenderDrawLineF(renderer, x, outline.y, std::min(x + dashLength, outline.x + outline.w), outline.y);
        }

        // 下边虚线
        for (float x = outline.x; x < outline.x + outline.w; x += dashLength + gapLength) {
            SDL_RenderDrawLineF(renderer, x, outline.y + outline.h,
                std::min(x + dashLength, outline.x + outline.w), outline.y + outline.h);
        }

        // 左边虚线
        for (float y = outline.y; y < outline.y + outline.h; y += dashLength + gapLength) {
            SDL_RenderDrawLineF(renderer, outline.x, y, outline.x, std::min(y + dashLength, outline.y + outline.h));
        }

        // 右边虚线
        for (float y = outline.y; y < outline.y + outline.h; y += dashLength + gapLength) {
            SDL_RenderDrawLineF(renderer, outline.x + outline.w, y,
                outline.x + outline.w, std::min(y + dashLength, outline.y + outline.h));
        }
    }

    // 恢复原始绘制颜色
    SDL_SetRenderDrawColor(renderer, originalColor.r, originalColor.g, originalColor.b, originalColor.a);
}

// 绘制圆形碰撞框
void ColliderComponent::DrawCircleCollider(SDL_Renderer* renderer, const Vector& center, float radius) {
    // 保存原始绘制颜色
    SDL_Color originalColor;
    SDL_GetRenderDrawColor(renderer, &originalColor.r, &originalColor.g, &originalColor.b, &originalColor.a);

    // 设置调试颜色
    SDL_SetRenderDrawColor(renderer, debugColor.r, debugColor.g, debugColor.b, debugColor.a);

    const int segments = 32; // 圆的细分段数
    const float angleStep = static_cast<const float>(2.0f * M_PI / segments);

    // 绘制圆形
    for (int i = 0; i < segments; i++) {
        float angle1 = i * angleStep;
        float angle2 = (i + 1) * angleStep;

        float x1 = center.x + radius * cos(angle1);
        float y1 = center.y + radius * sin(angle1);
        float x2 = center.x + radius * cos(angle2);
        float y2 = center.y + radius * sin(angle2);

        SDL_RenderDrawLineF(renderer, x1, y1, x2, y2);
    }

    // 如果是触发器，绘制十字线标识
    if (isTrigger) {
        SDL_RenderDrawLineF(renderer, center.x - radius, center.y, center.x + radius, center.y);
        SDL_RenderDrawLineF(renderer, center.x, center.y - radius, center.x, center.y + radius);
    }

    // 恢复原始绘制颜色
    SDL_SetRenderDrawColor(renderer, originalColor.r, originalColor.g, originalColor.b, originalColor.a);
}
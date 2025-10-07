#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "Component.h"
#include <memory>

Vector ColliderComponent::GetWorldPosition() const {
    if (auto transform = GetTransform()) {
        return transform->position + offset;
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
            worldPos.x - size.x * 0.5f,
            worldPos.y - size.y * 0.5f,
            size.x,
            size.y
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
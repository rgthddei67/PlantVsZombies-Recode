#include "Component.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"

void GameObject::RegisterAllColliders() {
    for (auto& [type, component] : components) {
        RegisterColliderIfNeeded(component);
    }
}

void GameObject::RegisterColliderIfNeeded(std::shared_ptr<Component> component) {
    if (auto collider = std::dynamic_pointer_cast<ColliderComponent>(component)) {
        CollisionSystem::GetInstance().RegisterCollider(collider);
    }
}

void GameObject::UnregisterColliderIfNeeded(std::shared_ptr<Component> component) {
    if (auto collider = std::dynamic_pointer_cast<ColliderComponent>(component)) {
        CollisionSystem::GetInstance().UnregisterCollider(collider);
    }
}
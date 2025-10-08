#include "GameObject.h"
#include "Component.h"
#include "../RendererManager.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"

GameObject::~GameObject() {
    for (auto& [type, component] : components) {
        UnregisterColliderIfNeeded(component);
        component->OnDestroy();
    }
    components.clear();
}

void GameObject::Start() {
    if (!started) {
        for (auto& component : componentsToInitialize) {
            InitializeComponent(component);
        }
        componentsToInitialize.clear();

        RegisterAllColliders();

        for (auto& [type, component] : components) {
            if (component->enabled) {
                component->Start();
            }
        }
        started = true;
    }
}

void GameObject::Update() {
    if (!active || !started) return;

    for (auto& [type, component] : components) {
        if (component->enabled) {
            component->Update();
        }
    }
}

void GameObject::Draw(SDL_Renderer* renderer) {
    if (!active || !started) return;

    for (auto& [type, component] : components) {
        if (component->enabled) {
            component->Draw(renderer);
        }
    }
}

void GameObject::InitializeComponent(std::shared_ptr<Component> component) {
    component->SetGameObject(shared_from_this());
    if (started && component->enabled) {
        component->Start();
    }
}

void GameObject::DestroyAllComponents() {
    for (auto& [type, component] : components) {
        component->OnDestroy();
    }
    components.clear();
    componentsToInitialize.clear();
}

SDL_Renderer* GameObject::GetRenderer() const {
    return RendererManager::GetInstance().GetRenderer();
}

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
#include "GameObject.h"
#include "Component.h"
#include "../RendererManager.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"

GameObject::~GameObject() 
{
    OnDestroy();
    for (auto& [type, component] : mComponents) {
        UnregisterColliderIfNeeded(component);
        component->OnDestroy();
    }
    mComponents.clear();
}

void GameObject::Start() {
    if (!mStarted) {
        for (auto& component : mComponentsToInitialize) {
            InitializeComponent(component);
        }
        mComponentsToInitialize.clear();

        RegisterAllColliders();

        for (auto& [type, component] : mComponents) {
            if (component->mEnabled) {
                component->Start();
            }
        }
        mStarted = true;
    }
}

void GameObject::Update() {
    if (!mActive || !mStarted) return;

    for (auto& [type, component] : mComponents) {
        if (component->mEnabled) {
            component->Update();
        }
    }
}

void GameObject::Draw(SDL_Renderer* renderer) {
    if (!mActive || !mStarted) return;

    for (auto& [type, component] : mComponents) {
        if (component->mEnabled) {
            component->Draw(renderer);
        }
    }
}

void GameObject::InitializeComponent(std::shared_ptr<Component> component) {
    component->SetGameObject(shared_from_this());
    if (mStarted && component->mEnabled) {
        component->Start();
    }
}

void GameObject::DestroyAllComponents() {
    for (auto& [type, component] : mComponents) {
        component->OnDestroy();
    }
    mComponents.clear();
    mComponentsToInitialize.clear();
}

SDL_Renderer* GameObject::GetRenderer() const {
    return RendererManager::GetInstance().GetRenderer();
}

void GameObject::RegisterAllColliders() {
    for (auto& [type, component] : mComponents) {
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
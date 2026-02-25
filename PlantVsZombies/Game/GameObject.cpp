#include "GameObject.h"
#include "Component.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"

GameObject::GameObject(ObjectType type)
    : mObjectType(type)
{
}

GameObject::~GameObject() 
{
    for (auto& [type, component] : mComponents) {
        UnregisterComponentIfNeeded(component);
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

void GameObject::Draw(Graphics* g) {
    if (!mActive || !mStarted) return;

    // 收集所有启用的组件
    std::vector<std::shared_ptr<Component>> componentsToDraw;
    componentsToDraw.reserve(mComponents.size());

    for (auto& [type, component] : mComponents) {
        if (component->mEnabled) {
            componentsToDraw.push_back(component);
        }
    }

    // 按绘制顺序排序：mDrawOrder 越大越先绘制（在底层）
    // 使用稳定排序，当 mDrawOrder 相等时保持原顺序（稳定排序）
    std::stable_sort(componentsToDraw.begin(), componentsToDraw.end(),
        [](const std::shared_ptr<Component>& a, const std::shared_ptr<Component>& b) {
            return a->GetDrawOrder() < b->GetDrawOrder();  // 降序排序
        });

    // 绘制组件
    for (auto& component : componentsToDraw) {
        component->Draw(g);
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
		UnregisterComponentIfNeeded(component);
        component->OnDestroy();
    }
    mComponents.clear();
    mComponentsToInitialize.clear();
}

void GameObject::RegisterAllColliders() {
    for (auto& [type, component] : mComponents) {
        RegisterComponentIfNeeded(component);
    }
}

void GameObject::RegisterComponentIfNeeded(std::shared_ptr<Component> component) {
    if (auto collider = std::dynamic_pointer_cast<ColliderComponent>(component)) {
        CollisionSystem::GetInstance().RegisterCollider(collider);
    }
}

void GameObject::UnregisterComponentIfNeeded(std::shared_ptr<Component> component) {
    if (auto collider = std::dynamic_pointer_cast<ColliderComponent>(component)) {
        CollisionSystem::GetInstance().UnregisterCollider(collider);
    }
}
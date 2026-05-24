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
		UnregisterComponentIfNeeded(component.get());
		component->OnDestroy();
	}
	mComponents.clear();
}

void GameObject::Start() {
	if (!mStarted) {
		for (auto* component : mComponentsToInitialize) {
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

	// 阶段三：仅 iterate mUpdatableComponents 视图（通常 size 0-1）
	// zombie/plant/bullet 上挂的 Transform/Collider/Shadow 都 NeedsUpdate()=false，视图为空，outer loop 直接退出
	for (Component* c : mUpdatableComponents) {
		if (c->mEnabled) c->Update();
	}
}

void GameObject::Draw(Graphics* g) {
	if (!mActive || !mStarted) return;

	// 收集所有启用的组件
	std::vector<Component*> componentsToDraw;
	componentsToDraw.reserve(mComponents.size());

	for (auto& [type, component] : mComponents) {
		if (component->mEnabled) {
			componentsToDraw.push_back(component.get());
		}
	}

	// 按绘制顺序排序：mDrawOrder 越大越先绘制（在底层）
	// 使用稳定排序，当 mDrawOrder 相等时保持原顺序（稳定排序）
	std::stable_sort(componentsToDraw.begin(), componentsToDraw.end(),
		[](const Component* a, const Component* b) {
			return a->GetDrawOrder() < b->GetDrawOrder();  // 降序排序
		});

	// 绘制组件
	for (auto* component : componentsToDraw) {
		component->Draw(g);
	}
}

void GameObject::InitializeComponent(Component* component) {
	component->SetGameObject(this);
	if (mStarted && component->mEnabled) {
		component->Start();
	}
}

void GameObject::DestroyAllComponents() {
	for (auto& [type, component] : mComponents) {
		UnregisterComponentIfNeeded(component.get());
		component->OnDestroy();
	}
	mComponents.clear();
	mComponentsToInitialize.clear();
	mUpdatableComponents.clear();
}

void GameObject::RegisterAllColliders() {
	for (auto& [type, component] : mComponents) {
		RegisterComponentIfNeeded(component.get());
	}
}

void GameObject::RegisterComponentIfNeeded(Component* component) {
	if (auto* collider = dynamic_cast<ColliderComponent*>(component)) {
		CollisionSystem::GetInstance().RegisterCollider(collider);
	}
}

void GameObject::UnregisterComponentIfNeeded(Component* component) {
	if (auto* collider = dynamic_cast<ColliderComponent*>(component)) {
		CollisionSystem::GetInstance().UnregisterCollider(collider);
	}
}
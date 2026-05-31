#include "GameObject.h"
#include "Component.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"

// 定义在此（而非 Component.h inline）：需要完整的 GameObject 类型来回调 MarkDrawableSortDirty，
// 而 Component.h 与 GameObject.h 互相依赖、无法在 inline 处看到完整 GameObject。
// 运行时改变绘制顺序会令 Draw 视图重排一次，保持旧代码"每帧重排"的语义。
void Component::SetDrawOrder(int order) {
	mDrawOrder = order;
	if (mGameObject) mGameObject->MarkDrawableSortDirty();
}

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

	// Draw 视图已在增删组件时预建。仅当 dirty（增删组件 / 运行时 SetDrawOrder）才重排一次，
	// 消除旧实现每帧 new vector + 遍历 unordered_map + stable_sort 的 per-frame 开销。
	// stable_sort 保证 mDrawOrder 相等时维持插入顺序，与旧行为一致。
	if (mDrawableSortDirty) {
		std::stable_sort(mDrawableComponents.begin(), mDrawableComponents.end(),
			[](const Component* a, const Component* b) {
				return a->GetDrawOrder() < b->GetDrawOrder();
			});
		mDrawableSortDirty = false;
	}

	// 禁用的组件留在视图里、在此被跳过——与旧实现"只收集 mEnabled"的可见结果完全一致。
	for (auto* component : mDrawableComponents) {
		if (component->mEnabled) {
			component->Draw(g);
		}
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
	mDrawableComponents.clear();
	mDrawableSortDirty = false;
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
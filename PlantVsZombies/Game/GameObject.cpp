#include "GameObject.h"
#include "Component.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "ShadowComponent.h"

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
	RemoveShadow();
	RemoveCollider();
	for (auto& [type, component] : mComponents) {
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

		RegisterColliderIfNeeded();

		for (auto& [type, component] : mComponents) {
			if (component->mEnabled) {
				component->Start();
			}
		}
		// Clickable 等附件可能在自己的 Start 中按需创建 Collider；再次幂等注册覆盖该路径。
		RegisterColliderIfNeeded();
		mStarted = true;
	}
}

void GameObject::Update() {
	if (!mActive || !mStarted) return;

	// 仅 iterate mUpdatableComponents 视图（通常 size 0-1）。Transform/Collider/Shadow 已由宿主显式拥有，
	// 当前只剩 Clickable 可能进入该视图；无可更新附件时直接退出。
	for (Component* c : mUpdatableComponents) {
		if (c->mEnabled) c->Update();
	}
}

void GameObject::Draw(Graphics* g) {
	if (!mActive || !mStarted) return;

	// 阴影固定先于宿主本体和剩余附件提交；Bullet 不调用本入口，改由 BulletPool 的地面阶段显式绘制。
	if (mShadow) mShadow->Draw(g);

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

	// Collider 不再进入 Component 绘制视图；仅在 Debug 开关开启时提交碰撞框。
	if (mCollider) mCollider->Draw(g);
}

void GameObject::InitializeComponent(Component* component) {
	component->SetGameObject(this);
	if (mStarted && component->mEnabled) {
		component->Start();
	}
}

void GameObject::DestroyAllComponents() {
	RemoveShadow();
	RemoveCollider();
	for (auto& [type, component] : mComponents) {
		component->OnDestroy();
	}
	mComponents.clear();
	mComponentsToInitialize.clear();
	mUpdatableComponents.clear();
	mDrawableComponents.clear();
	mDrawableSortDirty = false;
}

ColliderComponent* GameObject::CreateCollider(
	const Vector& size, const Vector& offset, ColliderType type) {
	RemoveCollider();
	mCollider = std::unique_ptr<ColliderComponent>(
		new ColliderComponent(this, size, offset, type));
	if (mStarted) RegisterColliderIfNeeded();
	return mCollider.get();
}

bool GameObject::RemoveCollider() {
	if (!mCollider) return false;
	CollisionSystem::GetInstance().UnregisterCollider(mCollider.get());
	mCollider.reset();
	return true;
}

ShadowComponent* GameObject::CreateShadow(
	const Texture* texture, const Vector& offset, float alpha) {
	mShadow = std::unique_ptr<ShadowComponent>(
		new ShadowComponent(this, texture, offset, alpha));
	return mShadow.get();
}

bool GameObject::RemoveShadow() {
	if (!mShadow) return false;
	mShadow.reset();
	return true;
}

void GameObject::RegisterColliderIfNeeded() {
	if (mCollider) CollisionSystem::GetInstance().RegisterCollider(mCollider.get());
}

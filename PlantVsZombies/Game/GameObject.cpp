#include "GameObject.h"
#include "Component.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "ShadowComponent.h"
#include "ClickableComponent.h"

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
	RemoveClickable();
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
		// 兼容仍可能在 Component::Start 中创建 Collider 的过渡代码；重复注册由 CollisionSystem 幂等处理。
		RegisterColliderIfNeeded();
		mStarted = true;
	}
}

void GameObject::Update() {
	if (!mActive || !mStarted) return;

	// Clickable 已脱离通用 Component 视图，但保持宿主更新后、场景统一命中处理前重置状态的时序。
	if (mClickable) mClickable->Update();

	// 迁移期保留通用 Component 视图；运行源码已无派生类，下一阶段统一删除框架。
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
	RemoveClickable();
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
	// 先完成新对象分配，再替换旧对象；这样已注册 Clickable 不会观察到半初始化 Collider。
	auto replacement = std::unique_ptr<ColliderComponent>(
		new ColliderComponent(this, size, offset, type));
	if (mCollider) CollisionSystem::GetInstance().UnregisterCollider(mCollider.get());
	mCollider = std::move(replacement);
	if (mStarted) RegisterColliderIfNeeded();
	return mCollider.get();
}

bool GameObject::RemoveCollider() {
	// Clickable 的命中几何由 Collider 唯一提供；显式移除几何时同步撤销点击注册。
	RemoveClickable();
	if (!mCollider) return false;
	CollisionSystem::GetInstance().UnregisterCollider(mCollider.get());
	mCollider.reset();
	return true;
}

ClickableComponent* GameObject::CreateClickable() {
	if (!mCollider) CreateCollider(Vector(50, 50));
	mCollider->isTrigger = true;
	RemoveClickable();
	mClickable = std::unique_ptr<ClickableComponent>(new ClickableComponent(this));
	return mClickable.get();
}

bool GameObject::RemoveClickable() {
	if (!mClickable) return false;
	mClickable.reset();
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

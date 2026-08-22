#pragma once
#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "RenderOrder.h"
#include "Transform.h"
#include "ColliderComponent.h"
#include "../Graphics.h"
#include "DeferredEvent.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <string>
#include <algorithm>
#include <optional>
#include <utility>

enum class ObjectType {
	OBJECT_NONE,
	OBJECT_UI,
	OBJECT_PLANT,
	OBJECT_ZOMBIE,
	OBJECT_BULLET,
	OBJECT_COIN,
	OBJECT_LAWNMOWER,
	OBJECT_PARTICLE,    // 可能废弃
};

class Component;
class ShadowComponent;
class ClickableComponent;

class GameObject {
public:
	bool mIsUI = false;
protected:
	ObjectType mObjectType = ObjectType::OBJECT_NONE;
	int mRenderOrder = LAYER_GAME_OBJECT;
	RenderLayer mLayer = LAYER_GAME_OBJECT;
	bool mActive = true; // 是否在活动
	bool mStarted = false;   // 标记
	bool mHasClipRect = false;
	ClipRect mClipRect;
	std::optional<Transform> mTransform; // 仅空间对象显式创建；非空间 UI/控制对象保持为空
	std::unique_ptr<ColliderComponent> mCollider; // 可选碰撞附件；由宿主独占并通过唯一入口注册/注销
	std::unique_ptr<ShadowComponent> mShadow; // 可选阴影附件；由宿主独占并在固定绘制阶段提交
	std::unique_ptr<ClickableComponent> mClickable; // 可选点击附件；由宿主独占并维护稀疏注册
	std::vector<Component*> mComponentsToInitialize; // 待初始化的组件（裸指针指向 mComponents 内的对象）
	std::unordered_map<std::type_index, std::unique_ptr<Component>> mComponents; // 包含的组件
	// 阶段三：仅缓存 NeedsUpdate()=true 的 Component 视图，避免每帧 iterate mComponents 全表
	// 非所有权 raw ptr 视图，所有权仍在 mComponents 的 unique_ptr
	std::vector<Component*> mUpdatableComponents;
	// Draw 视图：预建并按 mDrawOrder 预排序的可绘制组件，消除 GameObject::Draw 每帧
	// new vector / 遍历 unordered_map / stable_sort 三重 per-frame 开销（与 mUpdatableComponents 同构）。
	// 非所有权 raw ptr，所有权仍在 mComponents。mDrawableSortDirty：仅增删组件或 SetDrawOrder 时为真，懒排序。
	std::vector<Component*> mDrawableComponents;
	bool mDrawableSortDirty = false;
	std::string mTag = "Untagged";
	std::string mName = "GameObject";
	int mSortingKey = -1; // 可选的行深度键；普通对象保持 -1，按行残影可在构造期继承来源行

private:
	void RegisterColliderIfNeeded();

public:
	GameObject(ObjectType type = ObjectType::OBJECT_NONE);

	virtual ~GameObject();

	/** @brief 为当前宿主创建或重建唯一的空间值，并返回稳定的非拥有指针。 */
	template<typename... Args>
	Transform* CreateTransform(Args&&... args) {
		return &mTransform.emplace(std::forward<Args>(args)...);
	}

	Transform* GetTransform() {
		return mTransform ? &*mTransform : nullptr;
	}
	const Transform* GetTransform() const {
		return mTransform ? &*mTransform : nullptr;
	}

	/**
	 * @brief 创建或重建当前宿主唯一的 Collider。
	 * @details owner 绑定、旧 Collider 注销以及已启动宿主的注册均在此原子完成。
	 */
	ColliderComponent* CreateCollider(
		const Vector& size,
		const Vector& offset = Vector(0, 0),
		ColliderType type = ColliderType::BOX);

	ColliderComponent* GetCollider() { return mCollider.get(); }
	const ColliderComponent* GetCollider() const { return mCollider.get(); }

	/** @brief 注销并销毁当前宿主的 Collider；不存在时安全 no-op。 */
	bool RemoveCollider();

	/**
	 * @brief 创建或重建当前宿主唯一的点击附件。
	 * @details 缺少 Collider 时先创建 50x50 默认触发器，保证注册表内对象始终可安全命中测试。
	 */
	ClickableComponent* CreateClickable();
	ClickableComponent* GetClickable() { return mClickable.get(); }
	const ClickableComponent* GetClickable() const { return mClickable.get(); }
	/** @brief 注销并销毁当前宿主的点击附件；不存在时安全 no-op。 */
	bool RemoveClickable();

	/** @brief 创建或重建当前宿主唯一的阴影附件。 */
	ShadowComponent* CreateShadow(
		const Texture* texture = nullptr,
		const Vector& offset = Vector(0, 28),
		float alpha = 0.9f);
	ShadowComponent* GetShadow() { return mShadow.get(); }
	const ShadowComponent* GetShadow() const { return mShadow.get(); }
	/** @brief 销毁当前宿主的阴影附件；不存在时安全 no-op。 */
	bool RemoveShadow();

	// 添加组件 若是刚刚创建的对象，则不能使用，因为还没有
	template<typename T, typename... Args>
	T* AddComponent(Args&&... args) {
		static_assert(std::is_base_of<Component, T>::value, "T must be a Component");

		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* raw = component.get();

		auto typeIndex = std::type_index(typeid(T));
		mComponents[typeIndex] = std::move(component);
		if (raw->NeedsUpdate()) mUpdatableComponents.push_back(raw);
		// Draw 视图：所有组件都可能 Draw（默认 Draw() 为空），与旧实现一致地全部纳入；排序延后到首次 Draw。
		mDrawableComponents.push_back(raw);
		mDrawableSortDirty = true;

		mComponentsToInitialize.push_back(raw);

		// 如果对象已启动，立即初始化组件
		if (mStarted) {
			InitializeComponent(raw);
		}

		return raw;
	}

	// 获取组件
	template<typename T>
	T* GetComponent() {
		auto typeIndex = std::type_index(typeid(T));
		auto it = mComponents.find(typeIndex);
		if (it != mComponents.end()) {
			return static_cast<T*>(it->second.get());
		}
		return nullptr;
	}

	// 移除组件
	template<typename T>
	bool RemoveComponent() {
		auto typeIndex = std::type_index(typeid(T));
		auto it = mComponents.find(typeIndex);
		if (it != mComponents.end()) {
			it->second->OnDestroy();
			// 同步从 pending 列表中移除（防止删除后还有人对它做 InitializeComponent）
			mComponentsToInitialize.erase(
				std::remove(mComponentsToInitialize.begin(), mComponentsToInitialize.end(), it->second.get()),
				mComponentsToInitialize.end());
			// 阶段三：同步从 mUpdatableComponents 视图移除（vector 通常长度 0-1，扫描成本 O(0)~O(1)）
			mUpdatableComponents.erase(
				std::remove(mUpdatableComponents.begin(), mUpdatableComponents.end(), it->second.get()),
				mUpdatableComponents.end());
			// Draw 视图同步移除（移除不破坏剩余元素相对顺序，无需置 dirty）
			mDrawableComponents.erase(
				std::remove(mDrawableComponents.begin(), mDrawableComponents.end(), it->second.get()),
				mDrawableComponents.end());
			mComponents.erase(it);
			return true;
		}
		return false;
	}

	// 检查是否有某组件
	template<typename T>
	bool HasComponent() {
		return GetComponent<T>() != nullptr;
	}

	// 标记 Draw 视图需要重排：由 Component::SetDrawOrder 在运行时改变绘制顺序时回调，
	// 以保持"旧代码每帧重排"的语义（setup 期的 SetDrawOrder 已被 AddComponent 的置 dirty 覆盖）。
	void MarkDrawableSortDirty() { mDrawableSortDirty = true; }

	virtual void Start();

	virtual void Update();

	// 阶段二并行：默认空。约定——只做对象本地工作，worker 线程安全；
	//               遇到 deferred 操作 push 到 outBuf，主线程串行回放。
	virtual void UpdateParallel(std::vector<DeferredEvent>& outBuf) {}

	ObjectType GetObjectType() const { return mObjectType; }
	int GetRenderOrder() const { return mRenderOrder; }
	void SetRenderOrder(int order) { mRenderOrder = order; }
	RenderLayer GetLayer() const { return mLayer; }
	void SetLayer(RenderLayer layer) { mLayer = layer; }

	void SetClipRect(int x, int y, int w, int h) {
		mHasClipRect = true;
		mClipRect = { x, y, w, h };
	}
	void ClearClipRect() { mHasClipRect = false; }
	bool HasClipRect() const { return mHasClipRect; }
	const ClipRect& GetClipRect() const { return mClipRect; }
	virtual int GetSortingKey() const { return mSortingKey; } // 获取排序顺序，实现不同 row 顺序不一样
	void SetSortingKey(int sortingKey) { mSortingKey = sortingKey; }

	static RenderLayer GetLayerFromOrder(int renderOrder) {
		if (renderOrder < LAYER_GAME_OBJECT) return LAYER_BACKGROUND;
		else if (renderOrder < LAYER_GAME_PLANT) return LAYER_GAME_OBJECT;
		else if (renderOrder < LAYER_GAME_ZOMBIE) return LAYER_GAME_PLANT;
		else if (renderOrder < LAYER_GAME_BULLET) return LAYER_GAME_ZOMBIE;
		else if (renderOrder < LAYER_GAME_COIN) return LAYER_GAME_BULLET;
		else if (renderOrder < LAYER_EFFECTS) return LAYER_GAME_COIN;
		else if (renderOrder < LAYER_UI) return LAYER_EFFECTS;
		else if (renderOrder < LAYER_DEBUG) return LAYER_UI;
		else return LAYER_DEBUG;
	}

	// 绘制所有组件
	virtual void Draw(Graphics* g);

	// 获取物体的标签
	const std::string& GetTag() const { return mTag; }

	// 设置物体的标签
	void SetTag(const std::string& newTag) { mTag = newTag; }

	// 获取物体的名字
	const std::string& GetName() const { return mName; }

	// 设置物体的名字
	void SetName(const std::string& newName) { mName = newName; }

	// 获取物体的激活状态
	bool IsActive() const { return mActive; }
	bool HasStarted() const { return mStarted; }

	// 设置物体的激活状态
	void SetActive(bool state) { mActive = state; }

	// 初始化单个组件
	void InitializeComponent(Component* component);

	// 销毁所有组件
	void DestroyAllComponents();
};
#endif

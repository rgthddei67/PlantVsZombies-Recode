#pragma once
#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "RenderOrder.h"
#include "Transform.h"
#include "ColliderComponent.h"
#include "../InternedString.h"
#include "../Graphics.h"
#include "DeferredEvent.h"
#include <memory>
#include <vector>
#include <string>
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
	const std::string* mTag = nullptr; // 指向进程期驻留字符串；高数量实体不再各带 32B std::string
	const std::string* mName = nullptr; // 名称同样驻留；动态格子名仍按内容去重并保持稳定引用
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

	/** @brief 按固定顺序绘制宿主显式附件；派生类随后提交自身内容。 */
	virtual void Draw(Graphics* g);

	// 获取物体的标签
	const std::string& GetTag() const { return *mTag; }

	// 设置物体的标签
	void SetTag(const std::string& newTag) { mTag = &InternRuntimeString(newTag); }

	// 获取物体的名字
	const std::string& GetName() const { return *mName; }

	// 设置物体的名字
	void SetName(const std::string& newName) { mName = &InternRuntimeString(newName); }

	// 获取物体的激活状态
	bool IsActive() const { return mActive; }
	bool HasStarted() const { return mStarted; }

	// 设置物体的激活状态
	void SetActive(bool state) { mActive = state; }

	/**
	 * @brief 注销并销毁全部显式附件。
	 * @details GOM 在移除对象所有权时立即调用，避免外部 shared_ptr 延长对象生命并遗留输入或碰撞注册。
	 */
	void DestroyAttachments();
};
#endif

#pragma once
#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "RenderOrder.h"
#include "../Graphics.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <string>
#include <algorithm>

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
    std::vector<Component*> mComponentsToInitialize; // 待初始化的组件（裸指针指向 mComponents 内的对象）
    std::unordered_map<std::type_index, std::unique_ptr<Component>> mComponents; // 包含的组件
    std::string mTag = "Untagged";
    std::string mName = "GameObject";

private:
    void RegisterAllColliders();
    void RegisterComponentIfNeeded(Component* component);
    void UnregisterComponentIfNeeded(Component* component);

public:
    GameObject(ObjectType type = ObjectType::OBJECT_NONE);

    virtual ~GameObject();

    // 添加组件 若是刚刚创建的对象，则不能使用，因为还没有
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must be a Component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = component.get();

        auto typeIndex = std::type_index(typeid(T));
        mComponents[typeIndex] = std::move(component);

        mComponentsToInitialize.push_back(raw);

        // 如果对象已启动，立即初始化组件
        if (mStarted) {
            InitializeComponent(raw);
            RegisterComponentIfNeeded(raw);
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
            // 如果是碰撞器组件，从碰撞系统中注销
            // TODO: 以后若还有别的大系统，也要这么做
            UnregisterComponentIfNeeded(it->second.get());
            it->second->OnDestroy();
            // 同步从 pending 列表中移除（防止删除后还有人对它做 InitializeComponent）
            mComponentsToInitialize.erase(
                std::remove(mComponentsToInitialize.begin(), mComponentsToInitialize.end(), it->second.get()),
                mComponentsToInitialize.end());
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

    virtual void Start();

    virtual void Update();

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
    virtual int GetSortingKey() const { return -1; }        // 获取排序顺序，实现不同row顺序不一样

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

    // 设置物体的激活状态
    void SetActive(bool state) { mActive = state; }

    // 初始化单个组件
    void InitializeComponent(Component* component);

    // 销毁所有组件
    void DestroyAllComponents();

};
#endif

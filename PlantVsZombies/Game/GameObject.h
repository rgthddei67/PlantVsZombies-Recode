#pragma once
#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "../RendererManager.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <string>
#include <algorithm>
#include <SDL2/SDL.h>

class Component;

class GameObject : public std::enable_shared_from_this<GameObject> {
private:
    std::vector<std::shared_ptr<Component>> componentsToInitialize; // 待初始化的组件
    std::unordered_map<std::type_index, std::shared_ptr<Component>> components; // 包含的组件
    std::string tag = "Untagged";
    std::string name = "GameObject";
    bool active = true; // 是否在活动
    bool started = false;   // 标记
    void RegisterAllColliders();
    void RegisterColliderIfNeeded(std::shared_ptr<Component> component);
    void UnregisterColliderIfNeeded(std::shared_ptr<Component> component);

public:
    virtual ~GameObject();

    // 添加组件 若是刚刚创建的对象，则不能使用，因为还没有
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must be a Component");

        auto component = std::make_shared<T>(std::forward<Args>(args)...);

        auto typeIndex = std::type_index(typeid(T));
        components[typeIndex] = component;

        // 延迟设置GameObject，避免在构造函数中调用shared_from_this() 造成bad_weak_ptr错误
        // TODO: 警告: 在构造函数中不要调用 shared_from_this()
        componentsToInitialize.push_back(component);

        // 如果对象已启动，立即初始化组件
        if (started) {
            InitializeComponent(component);
            RegisterColliderIfNeeded(component);
        }

        return component;
    }

    // 获取组件
    template<typename T>
    std::shared_ptr<T> GetComponent() {
        auto typeIndex = std::type_index(typeid(T));
        auto it = components.find(typeIndex);
        if (it != components.end()) {
            auto component = std::static_pointer_cast<T>(it->second);
            // 检查组件是否还有效
            if (component->GetGameObject()) {
                return component;
            }
        }
        return nullptr;
    }

    // 移除组件
    template<typename T>
    bool RemoveComponent() {
        auto typeIndex = std::type_index(typeid(T));
        auto it = components.find(typeIndex);
        if (it != components.end()) {
            // 如果是碰撞器组件，从碰撞系统中注销
            // TOOD: 以后若还有别的大系统，也要这么做
            UnregisterColliderIfNeeded(it->second);
            it->second->OnDestroy();
            components.erase(it);
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

    // 绘制所有组件（如果组件要绘制的话)
    virtual void Draw(SDL_Renderer* renderer);

    // 获取物体的标签
    const std::string& GetTag() const { return tag; }

    // 设置物体的标签
    void SetTag(const std::string& newTag) { tag = newTag; }

    // 获取物体的名字
    const std::string& GetName() const { return name; }

    // 设置物体的名字
    void SetName(const std::string& newName) { name = newName; }

    // 获取物体的激活状态
    bool IsActive() const { return active; }

    // 设置物体的激活状态
    void SetActive(bool state) { active = state; }

    // 初始化单个组件
    void InitializeComponent(std::shared_ptr<Component> component);

    // 销毁所有组件
    void DestroyAllComponents();

    // 获取渲染器
    SDL_Renderer* GetRenderer() const;
};
#endif
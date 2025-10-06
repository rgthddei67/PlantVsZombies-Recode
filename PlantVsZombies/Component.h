#pragma once
#ifndef _COMPONENT_H
#define _COMPONENT_H

#include "Definit.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <functional>
#include <vector>

class GameObject;

class Component : public std::enable_shared_from_this<Component> {
public:
	std::weak_ptr<GameObject> gameObjectWeak;   // 不能shared_ptr 防止循环引用(GameObject)
    bool enabled = true;

    virtual ~Component() = default;

    // 生命周期方法
    virtual void Start() {}           // 组件开始时调用
    virtual void Update() {}          // 每帧更新
    virtual void OnDestroy() {}       // 组件销毁时调用

    // 获取 GameObject（如果还存在）
    std::shared_ptr<GameObject> GetGameObject() const {
        return gameObjectWeak.lock();
    }
    // 设置所属游戏对象
    void SetGameObject(std::shared_ptr<GameObject> obj) {
        gameObjectWeak = obj;
    }
};

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
    virtual ~GameObject() 
    {
        for (auto& [type, component] : components) {
            component->OnDestroy();
        }
        components.clear();
    }

    // 添加组件
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

    // 启动对象并调用所有组件的Start
    void Start() {
        if (!started) {
            for (auto& component : componentsToInitialize) {
                InitializeComponent(component);
            }
            componentsToInitialize.clear();
            // 注册所有碰撞器组件到碰撞系统
            // TOOD: 以后若还有别的大系统，也要这么做
            RegisterAllColliders();
            for (auto& [type, component] : components) {
                if (component->enabled) {
                    component->Start();
                }
            }
            started = true;
        }
    }

    virtual void Update() {
        if (!active || !started) return;

        for (auto& [type, component] : components) {
            if (component->enabled) {
                component->Update();
            }
        }
    }

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
    void InitializeComponent(std::shared_ptr<Component> component) {
        component->SetGameObject(shared_from_this());
        if (started && component->enabled) {
            component->Start();
        }
    }
	// 销毁所有组件
    void DestroyAllComponents() {
        for (auto& [type, component] : components) {
            component->OnDestroy();
        }
        components.clear();
        componentsToInitialize.clear();
    }
};

class GameObjectManager {
private:
	std::vector<std::shared_ptr<GameObject>> gameObjects;       // 已经有的游戏对象
	std::vector<std::shared_ptr<GameObject>> objectsToAdd;      // 待添加的游戏对象
	std::vector<std::shared_ptr<GameObject>> objectsToRemove;   // 待删除的游戏对象

public:
    static GameObjectManager& GetInstance() {
        static GameObjectManager instance;
        return instance;
    }

	// 创建游戏对象 (塞入objectsToAdd，在Update时执行Start)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObject(Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        objectsToAdd.push_back(obj);
        return obj;
    }

	// 立即创建游戏对象并启动（立刻调用Start 并且塞入gameObjects 而不是objectsToAdd)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObjectImmediate(Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        gameObjects.push_back(obj);
        obj->Start();
        return obj;
    }

    // 销毁游戏对象
    void DestroyGameObject(std::shared_ptr<GameObject> obj) 
    {
        if (obj) {
            objectsToRemove.push_back(obj);
        }
    }

    void Update() {
        // 移除在objectsToRemove中的对象
        for (auto& objToRemove : objectsToRemove) {
            if (objToRemove) {
                objToRemove->DestroyAllComponents();
            }
            gameObjects.erase(
                std::remove(gameObjects.begin(), gameObjects.end(), objToRemove),
                gameObjects.end()
            );
        }
        objectsToRemove.clear();

		// 新对象的操作
        for (auto& obj : objectsToAdd) {
            gameObjects.push_back(obj);
            obj->Start();
        }
        objectsToAdd.clear();

        // 更新现有（gameObjects）对象
        for (auto& obj : gameObjects) {
            if (obj->IsActive()) {
                obj->Update();
            }
        }
    }

    // 查找在gameObjects中的符合条件游戏对象 (根据tag标签)
    std::vector<std::shared_ptr<GameObject>> FindGameObjectsWithTag(const std::string& tag) {
        std::vector<std::shared_ptr<GameObject>> result;
        for (auto& obj : gameObjects) {
            if (obj->GetTag() == tag) {
                result.push_back(obj);
            }
        }
        return result;
    }

    // 查找在gameObjects中的第一个符合条件游戏对象 (根据tag标签)
    std::shared_ptr<GameObject> FindGameObjectWithTag(const std::string& tag) {
        auto objects = FindGameObjectsWithTag(tag);
        return objects.empty() ? nullptr : objects[0];
    }

    // 清空所有对象
    void ClearAll() {
        // 先销毁所有对象
        for (auto& obj : gameObjects) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        gameObjects.clear();

        for (auto& obj : objectsToAdd) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        objectsToAdd.clear();

        for (auto& obj : objectsToRemove) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        objectsToRemove.clear();
    }
};

#endif
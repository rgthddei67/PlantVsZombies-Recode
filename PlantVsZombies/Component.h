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
	std::weak_ptr<GameObject> gameObjectWeak;   // ����shared_ptr ��ֹѭ������(GameObject)
    bool enabled = true;

    virtual ~Component() = default;

    // �������ڷ���
    virtual void Start() {}           // �����ʼʱ����
    virtual void Update() {}          // ÿ֡����
    virtual void OnDestroy() {}       // �������ʱ����

    // ��ȡ GameObject����������ڣ�
    std::shared_ptr<GameObject> GetGameObject() const {
        return gameObjectWeak.lock();
    }
    // ����������Ϸ����
    void SetGameObject(std::shared_ptr<GameObject> obj) {
        gameObjectWeak = obj;
    }
};

class GameObject : public std::enable_shared_from_this<GameObject> {
private:
	std::vector<std::shared_ptr<Component>> componentsToInitialize; // ����ʼ�������
	std::unordered_map<std::type_index, std::shared_ptr<Component>> components; // ���������
    std::string tag = "Untagged";  
    std::string name = "GameObject";
	bool active = true; // �Ƿ��ڻ
	bool started = false;   // ���
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

    // ������
    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must be a Component");

        auto component = std::make_shared<T>(std::forward<Args>(args)...);

        auto typeIndex = std::type_index(typeid(T));
        components[typeIndex] = component;

        // �ӳ�����GameObject�������ڹ��캯���е���shared_from_this() ���bad_weak_ptr����
        // TODO: ����: �ڹ��캯���в�Ҫ���� shared_from_this()
        componentsToInitialize.push_back(component);

        // ���������������������ʼ�����
        if (started) {
            InitializeComponent(component);
            RegisterColliderIfNeeded(component);
        }

        return component;
    }

    // ��ȡ���
    template<typename T>
    std::shared_ptr<T> GetComponent() {
        auto typeIndex = std::type_index(typeid(T));
        auto it = components.find(typeIndex);
        if (it != components.end()) {
            auto component = std::static_pointer_cast<T>(it->second);
            // �������Ƿ���Ч
            if (component->GetGameObject()) {
                return component;
            }
        }
        return nullptr;
    }

    // �Ƴ����
    template<typename T>
    bool RemoveComponent() {
        auto typeIndex = std::type_index(typeid(T));
        auto it = components.find(typeIndex);
        if (it != components.end()) {
            // �������ײ�����������ײϵͳ��ע��
            // TOOD: �Ժ������б�Ĵ�ϵͳ��ҲҪ��ô��
            UnregisterColliderIfNeeded(it->second);
            it->second->OnDestroy();
            components.erase(it);
            return true;
        }
        return false;
    }

    // ����Ƿ���ĳ���
    template<typename T>
    bool HasComponent() {
        return GetComponent<T>() != nullptr;
    }

    // �������󲢵������������Start
    void Start() {
        if (!started) {
            for (auto& component : componentsToInitialize) {
                InitializeComponent(component);
            }
            componentsToInitialize.clear();
            // ע��������ײ���������ײϵͳ
            // TOOD: �Ժ������б�Ĵ�ϵͳ��ҲҪ��ô��
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

    // ��ȡ����ı�ǩ
    const std::string& GetTag() const { return tag; }
    // ��������ı�ǩ
    void SetTag(const std::string& newTag) { tag = newTag; }
    // ��ȡ���������
    const std::string& GetName() const { return name; }
    // �������������
    void SetName(const std::string& newName) { name = newName; }
	// ��ȡ����ļ���״̬
    bool IsActive() const { return active; }
	// ��������ļ���״̬
    void SetActive(bool state) { active = state; }
    // ��ʼ���������
    void InitializeComponent(std::shared_ptr<Component> component) {
        component->SetGameObject(shared_from_this());
        if (started && component->enabled) {
            component->Start();
        }
    }
	// �����������
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
	std::vector<std::shared_ptr<GameObject>> gameObjects;       // �Ѿ��е���Ϸ����
	std::vector<std::shared_ptr<GameObject>> objectsToAdd;      // ����ӵ���Ϸ����
	std::vector<std::shared_ptr<GameObject>> objectsToRemove;   // ��ɾ������Ϸ����

public:
    static GameObjectManager& GetInstance() {
        static GameObjectManager instance;
        return instance;
    }

	// ������Ϸ���� (����objectsToAdd����Updateʱִ��Start)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObject(Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        objectsToAdd.push_back(obj);
        return obj;
    }

	// ����������Ϸ�������������̵���Start ��������gameObjects ������objectsToAdd)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObjectImmediate(Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        gameObjects.push_back(obj);
        obj->Start();
        return obj;
    }

    // ������Ϸ����
    void DestroyGameObject(std::shared_ptr<GameObject> obj) 
    {
        if (obj) {
            objectsToRemove.push_back(obj);
        }
    }

    void Update() {
        // �Ƴ���objectsToRemove�еĶ���
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

		// �¶���Ĳ���
        for (auto& obj : objectsToAdd) {
            gameObjects.push_back(obj);
            obj->Start();
        }
        objectsToAdd.clear();

        // �������У�gameObjects������
        for (auto& obj : gameObjects) {
            if (obj->IsActive()) {
                obj->Update();
            }
        }
    }

    // ������gameObjects�еķ���������Ϸ���� (����tag��ǩ)
    std::vector<std::shared_ptr<GameObject>> FindGameObjectsWithTag(const std::string& tag) {
        std::vector<std::shared_ptr<GameObject>> result;
        for (auto& obj : gameObjects) {
            if (obj->GetTag() == tag) {
                result.push_back(obj);
            }
        }
        return result;
    }

    // ������gameObjects�еĵ�һ������������Ϸ���� (����tag��ǩ)
    std::shared_ptr<GameObject> FindGameObjectWithTag(const std::string& tag) {
        auto objects = FindGameObjectsWithTag(tag);
        return objects.empty() ? nullptr : objects[0];
    }

    // ������ж���
    void ClearAll() {
        // ���������ж���
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
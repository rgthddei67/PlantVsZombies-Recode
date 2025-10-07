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
    virtual ~GameObject();

    // ������ ���Ǹոմ����Ķ�������ʹ�ã���Ϊ��û��
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

    virtual void Start();

    virtual void Update();

    // �������������������Ҫ���ƵĻ�)
    virtual void Draw(SDL_Renderer* renderer);

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
    void InitializeComponent(std::shared_ptr<Component> component);

    // �����������
    void DestroyAllComponents();

    // ��ȡ��Ⱦ��
    SDL_Renderer* GetRenderer() const;
};
#endif
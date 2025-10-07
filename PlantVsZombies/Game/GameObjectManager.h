#pragma once
#ifndef _GAMEOBJECTMANAGER_H
#define _GAMEOBJECTMANAGER_H
#include <vector>
#include <memory>
#include <algorithm>
#include <SDL2/SDL.h>
#include "GameObject.h"

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

    // ��������GameObject����
    void DrawAll(SDL_Renderer* renderer) {
        for (auto& obj : gameObjects) {
            if (obj->IsActive()) {
                obj->Draw(renderer);
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
    
    // ��ȡgameObjects����
    const std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() const {
        return gameObjects;
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
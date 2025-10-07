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

    // 绘制所有GameObject对象
    void DrawAll(SDL_Renderer* renderer) {
        for (auto& obj : gameObjects) {
            if (obj->IsActive()) {
                obj->Draw(renderer);
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
    
    // 获取gameObjects引用
    const std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() const {
        return gameObjects;
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
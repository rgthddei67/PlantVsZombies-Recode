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
    std::vector<std::shared_ptr<GameObject>> mGameObjects;       // 已经有的游戏对象
    std::vector<std::shared_ptr<GameObject>> mObjectsToAdd;      // 待添加的游戏对象
    std::vector<std::shared_ptr<GameObject>> mObjectsToRemove;   // 待删除的游戏对象

public:
    static GameObjectManager& GetInstance() {
        static GameObjectManager instance;
        return instance;
    }

    // 创建游戏对象 (塞入mObjectsToAdd，在Update时执行Start)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObject(Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        mObjectsToAdd.push_back(obj);
        return obj;
    }

    // 立即创建游戏对象并启动（立刻调用Start 并且塞入mGameObjects 而不是mObjectsToAdd)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObjectImmediate(Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        mGameObjects.push_back(obj);
        obj->Start();
        return obj;
    }

    // 销毁游戏对象
    void DestroyGameObject(std::shared_ptr<GameObject> obj)
    {
        if (obj) {
            mObjectsToRemove.push_back(obj);
        }
    }

    // 销毁全部游戏对象
    void DestroyAllGameObjects() {
        // 销毁所有现有对象
        for (auto& obj : mGameObjects) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        mGameObjects.clear();

        // 销毁待添加对象
        for (auto& obj : mObjectsToAdd) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        mObjectsToAdd.clear();

        // 销毁待删除对象
        for (auto& obj : mObjectsToRemove) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        mObjectsToRemove.clear();

#ifdef _DEBUG
        std::cout << "GameObjectManager::DestroyAllGameObjects 已销毁所有游戏对象" << std::endl;
#endif
    }

    void Update() {
        // 移除在mObjectsToRemove中的对象
        for (auto& objToRemove : mObjectsToRemove) {
            if (objToRemove) {
                objToRemove->DestroyAllComponents();
            }
            mGameObjects.erase(
                std::remove(mGameObjects.begin(), mGameObjects.end(), objToRemove),
                mGameObjects.end()
            );
        }
        mObjectsToRemove.clear();

        // 新对象的操作
        for (auto& obj : mObjectsToAdd) {
            mGameObjects.push_back(obj);
            obj->Start();
        }
        mObjectsToAdd.clear();

        // 更新现有（mGameObjects）对象
        for (auto& obj : mGameObjects) {
            if (obj->IsActive()) {
                obj->Update();
            }
        }
    }

    // 绘制所有GameObject对象
    void DrawAll(SDL_Renderer* renderer) {
        // 按渲染顺序排序
        std::sort(mGameObjects.begin(), mGameObjects.end(),
            [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
                return a->GetRenderOrder() < b->GetRenderOrder();
            });

        // 绘制所有游戏对象
        for (auto& obj : mGameObjects) {
            if (obj->IsActive()) {
                obj->Draw(renderer);
            }
        }
    }

    // 查找在gameObjects中的符合条件游戏对象 (根据tag标签)
    std::vector<std::shared_ptr<GameObject>> FindGameObjectsWithTag(const std::string& tag) {
        std::vector<std::shared_ptr<GameObject>> result;
        for (auto& obj : mGameObjects) {
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
        return mGameObjects;
    }

    // 清空所有对象
    void ClearAll() {
        // 先销毁所有对象
        for (auto& obj : mGameObjects) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        mGameObjects.clear();

        for (auto& obj : mObjectsToAdd) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        mObjectsToAdd.clear();

        for (auto& obj : mObjectsToRemove) {
            if (obj) {
                obj->DestroyAllComponents();
            }
        }
        mObjectsToRemove.clear();
    }
};
#endif
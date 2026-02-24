#pragma once
#ifndef _GAMEOBJECTMANAGER_H
#define _GAMEOBJECTMANAGER_H

#include <vector>
#include <memory>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include "GameObject.h"

const int SUBORDER_PER_KEY = 1000;  // 每个key最多同时存在的顺序数量

class GameObjectManager {
private:
    // 每个图层有自己的回收池（全局）
    std::map<RenderLayer, std::set<int>> mLayerRecycledOrders;
    // 每个图层当前的最大子顺序
    std::map<RenderLayer, int> mLayerMaxSubOrder;

    // 按排序键（如行号）管理的池
    std::map<RenderLayer, std::map<int, std::set<int>>> mLayerKeyRecycledOrders;  // 空闲子顺序（全局值）
    std::map<RenderLayer, std::map<int, int>> mLayerKeyMaxLocalIdx;  // 当前已分配的本地索引（0 ~ SUBORDER_PER_KEY-1）

    std::vector<std::shared_ptr<GameObject>> mGameObjects;       // 已经有的游戏对象
    std::vector<std::shared_ptr<GameObject>> mObjectsToAdd;      // 待添加的游戏对象
    std::vector<std::shared_ptr<GameObject>> mObjectsToRemove;   // 待删除的游戏对象

public:
    static GameObjectManager& GetInstance() {
        static GameObjectManager instance;
        return instance;
    }

    GameObjectManager()
    {
		ResetAllLayers();
    }

    // 创建游戏对象 (塞入mObjectsToAdd，在Update时执行Start)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObject(RenderLayer layer, Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        obj->SetLayer(layer);
        AssignRenderOrder(obj, layer);
        mObjectsToAdd.push_back(obj);
        return obj;
    }

    // 立即创建游戏对象并启动（立刻调用Start 并且塞入mGameObjects 而不是mObjectsToAdd)
    template<typename T, typename... Args>
    std::shared_ptr<T> CreateGameObjectImmediate(RenderLayer layer, Args&&... args) {
        static_assert(std::is_base_of<GameObject, T>::value, "T must be a GameObject");
        auto obj = std::make_shared<T>(std::forward<Args>(args)...);
        obj->SetLayer(layer);
        AssignRenderOrder(obj, layer);
        mGameObjects.push_back(obj);
        obj->Start();
        return obj;
    }

    // 销毁游戏对象
    void DestroyGameObject(std::shared_ptr<GameObject> obj) {
        if (obj) {
            RecycleRenderOrder(obj->GetRenderOrder(), obj->GetLayer(), obj->GetSortingKey());
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

        ResetAllLayers();

#ifdef _DEBUG
        std::cout << "GameObjectManager::DestroyAllGameObjects 已销毁所有游戏对象" << std::endl;
#endif
    }

    // 更新
    void Update() {
        // 移除在mObjectsToRemove中的对象
        for (size_t i = 0; i < mObjectsToRemove.size(); i++) {
            auto obj = mObjectsToRemove[i];
            if (obj) {
                obj->DestroyAllComponents();
            }
            mGameObjects.erase(
                std::remove(mGameObjects.begin(), mGameObjects.end(), obj),
                mGameObjects.end()
            );
		}
        mObjectsToRemove.clear();

        // 新对象的操作
        for (size_t i = 0; i < mObjectsToAdd.size(); i++) {
            auto obj = mObjectsToAdd[i];
            mGameObjects.push_back(obj);
            obj->Start();
		}

        mObjectsToAdd.clear();

        /*
        * TODO: 一定不要存vector元素的&!!!!!!一定不要存vector元素的&!!!!!!一定不要存vector元素的&!!!！
        * shared_ptr指向堆上的 GameObject 对象
        e.g.
        GameObject* rawPtr = mGameObjects[0].get();  // 指向堆上的 GameObject
        rawPtr 指向的是由 shared_ptr 管理的堆对象。

        vector 扩容只移动 shared_ptr 本身，堆对象一动不动。

        所以 rawPtr 始终有效（只要对象未被销毁）。
        */

        // 更新现有（mGameObjects）对象
        for (size_t i = 0; i < mGameObjects.size(); i++) {
            auto obj = mGameObjects[i].get();
            if (obj->IsActive()) {
                obj->Update();
            }
        }
    }

    // 绘制所有GameObject对象
    void DrawAll(Graphics* g) {
        // 按渲染顺序排序
        std::sort(mGameObjects.begin(), mGameObjects.end(),
            [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
                return a->GetRenderOrder() < b->GetRenderOrder();
            });

        // 绘制所有游戏对象
        for (size_t i = 0; i < mGameObjects.size(); ++i) {
            auto* obj = mGameObjects[i].get(); 
            if (obj->IsActive()) {
                obj->Draw(g);
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
        
        ResetAllLayers();
    }

    // 初始化所有图层
    void ResetAllLayers() {
        mLayerRecycledOrders.clear();
        mLayerMaxSubOrder.clear();
        mLayerKeyRecycledOrders.clear();
        mLayerKeyMaxLocalIdx.clear();

        // 初始化所有枚举值
        RenderLayer layers[] = {
            LAYER_BACKGROUND,
            LAYER_GAME_OBJECT,
            LAYER_GAME_PLANT,
            LAYER_GAME_ZOMBIE,
            LAYER_GAME_BULLET,
            LAYER_GAME_COIN,
            LAYER_EFFECTS,
            LAYER_UI,
            LAYER_DEBUG
        };

        for (RenderLayer layer : layers) {
            ResetLayer(layer);
        }
    }

    void AssignRenderOrder(std::shared_ptr<GameObject> gameObject, RenderLayer layer) {
        // 先回收旧的渲染顺序（需要知道旧的 key）
        RecycleRenderOrder(gameObject->GetRenderOrder(), gameObject->GetLayer(), gameObject->GetSortingKey());

        int subOrder;
        int key = gameObject->GetSortingKey();
        if (key >= 0) {
            subOrder = GetNextSubOrderForKey(layer, key);
        }
        else {
            subOrder = GetNextSubOrder(layer);
        }
        int renderOrder = static_cast<int>(layer) + subOrder;
        gameObject->SetRenderOrder(renderOrder);
    }

    // 回收渲染顺序
    void RecycleRenderOrder(int renderOrder, RenderLayer layer, int key = -1) {
        int subOrder = renderOrder - static_cast<int>(layer);
        if (subOrder >= 0 && subOrder < 10000) {  // 子顺序范围 0~9999
            if (key >= 0) {
                mLayerKeyRecycledOrders[layer][key].insert(subOrder);
            }
            else {
                mLayerRecycledOrders[layer].insert(subOrder);
            }
        }
    }

private:
    // 获取图层内的下一个可用子顺序
    int GetNextSubOrder(RenderLayer layer) {
        // 1. 优先从回收池获取
        if (!mLayerRecycledOrders[layer].empty()) {
            int recycled = *mLayerRecycledOrders[layer].begin();
            mLayerRecycledOrders[layer].erase(mLayerRecycledOrders[layer].begin());
            return recycled;
        }

        // 2. 分配新的子顺序
        int newOrder = mLayerMaxSubOrder[layer]++;

        // 3. 检查是否超过图层容量
        if (newOrder >= 10000) { // 每个图层最多10000个子顺序
            // 尝试从回收池获取
            if (!mLayerRecycledOrders[layer].empty()) {
                newOrder = *mLayerRecycledOrders[layer].begin();
                mLayerRecycledOrders[layer].erase(mLayerRecycledOrders[layer].begin());
            }
            else {
                // 重置该图层
                ResetLayer(layer);
                newOrder = mLayerMaxSubOrder[layer]++; // 从0开始
            }
        }

        return newOrder;
    }

    // 按key分配下一个可用子顺序
    int GetNextSubOrderForKey(RenderLayer layer, int key) {
        // 1. 优先从该 key 的空闲池中取
        auto& recycled = mLayerKeyRecycledOrders[layer][key];
        if (!recycled.empty()) {
            int sub = *recycled.begin();
            recycled.erase(recycled.begin());
            return sub;
        }

        // 2. 分配新区间内的本地索引
        int localIdx = mLayerKeyMaxLocalIdx[layer][key];  // 默认初始为 0
        if (localIdx < SUBORDER_PER_KEY) {
            int globalSub = key * SUBORDER_PER_KEY + localIdx;
            mLayerKeyMaxLocalIdx[layer][key] = localIdx + 1;
            return globalSub;
        }

        // 3. 超出区间上限，尝试再次检查回收池
        if (!recycled.empty()) {
            int sub = *recycled.begin();
            recycled.erase(recycled.begin());
            return sub;
        }

        // 4. 极端情况：重置该 key 的区间（清空回收池，从 0 重新开始）
        // 注意：这会导致之前已分配但尚未回收的子顺序被废弃，但概率极低
        ResetKeyLayer(layer, key);
        int globalSub = key * SUBORDER_PER_KEY;  // 从 0 开始
        mLayerKeyMaxLocalIdx[layer][key] = 1;    // 下一个可用本地索引为 1
        return globalSub;
    }

    void ResetKeyLayer(RenderLayer layer, int key) {
        mLayerKeyMaxLocalIdx[layer][key] = 0;
        mLayerKeyRecycledOrders[layer][key].clear();
    }

    // 重置指定图层
    void ResetLayer(RenderLayer layer) {
        mLayerMaxSubOrder[layer] = 0;
        mLayerRecycledOrders[layer].clear();
    }

};

#endif
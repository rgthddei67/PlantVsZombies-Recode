#pragma once
#ifndef _GAMEOBJECTMANAGER_H
#define _GAMEOBJECTMANAGER_H

#include <vector>
#include <memory>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include <thread>
#include "GameObject.h"
#include "ThreadPool.h"
#include "ObjectPool/BulletPool.h"

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

    std::unique_ptr<ThreadPool> mThreadPool;
    bool mSortDirty = true;

    // 对象池
    std::unique_ptr<BulletPool> mBulletPool;

public:
    static GameObjectManager& GetInstance() {
        static GameObjectManager instance;
        return instance;
    }

    GameObjectManager();

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
        mSortDirty = true;   // 直接加入 mGameObjects，需要重新排序
        obj->Start();
        return obj;
    }

    // 销毁游戏对象
    void DestroyGameObject(std::shared_ptr<GameObject> obj);

    // 销毁全部游戏对象
    void DestroyAllGameObjects();

    // 更新
    void Update();

    // 绘制所有GameObject对象
    void DrawAll(Graphics* g);

    // 查找在gameObjects中的符合条件游戏对象 (根据tag标签)
    std::vector<std::shared_ptr<GameObject>> FindGameObjectsWithTag(const std::string& tag);

    // 查找在gameObjects中的第一个符合条件游戏对象 (根据tag标签)
    std::shared_ptr<GameObject> FindGameObjectWithTag(const std::string& tag);

    // 获取gameObjects引用
    const std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() const { return mGameObjects; }

    // 清空所有对象
    void ClearAll();

    // 获取对象池
    BulletPool* GetBulletPool() { return mBulletPool.get(); }

    // 打印对象池统计信息
    void PrintPoolStats() const;

    // 初始化所有图层
    void ResetAllLayers();

    void AssignRenderOrder(std::shared_ptr<GameObject> gameObject, RenderLayer layer);

    // 回收渲染顺序
    void RecycleRenderOrder(int renderOrder, RenderLayer layer, int key = -1);

private:
    // 获取图层内的下一个可用子顺序
    int GetNextSubOrder(RenderLayer layer);

    // 按key分配下一个可用子顺序
    int GetNextSubOrderForKey(RenderLayer layer, int key);

    void ResetKeyLayer(RenderLayer layer, int key);

    // 重置指定图层
    void ResetLayer(RenderLayer layer);
};

#endif
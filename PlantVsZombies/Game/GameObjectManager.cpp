#include "GameObjectManager.h"

GameObjectManager::GameObjectManager() {
    ResetAllLayers();
    int n = static_cast<int>(std::thread::hardware_concurrency());
    if (n < 1) n = 1;
    mThreadPool = std::make_unique<ThreadPool>(n);

    mGameObjects.reserve(2048);
    mObjectsToAdd.reserve(256);
    mObjectsToRemove.reserve(2048);

    // 初始化对象池
    mBulletPool = std::make_unique<BulletPool>();
    mBulletPool->Initialize(300, 600);  // 初始容量 300，警告阈值 600
}

void GameObjectManager::DestroyGameObject(std::shared_ptr<GameObject> obj) {
    if (obj) {
        RecycleRenderOrder(obj->GetRenderOrder(), obj->GetLayer(), obj->GetSortingKey());
        mObjectsToRemove.push_back(obj);
    }
}

void GameObjectManager::DestroyAllGameObjects() {
    mBulletPool->Clear();

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

#ifdef _DEBUG
    std::cout << "GameObjectManager::DestroyAllGameObjects 已销毁所有游戏对象" << std::endl;
#endif
}

void GameObjectManager::Update() {
    // 有增删时标记排序脏
    if (!mObjectsToRemove.empty() || !mObjectsToAdd.empty())
        mSortDirty = true;

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

    // 现有对象
    for (size_t i = 0; i < mGameObjects.size(); i++) {
        auto obj = mGameObjects[i].get();
        if (obj->IsActive()) {
            obj->Update();
        }
    }
}

void GameObjectManager::DrawAll(Graphics* g) {
    // 按渲染顺序排序（仅在有增删时重新排序）
    if (mSortDirty) {
        std::sort(mGameObjects.begin(), mGameObjects.end(),
            [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
                return a->GetRenderOrder() < b->GetRenderOrder();
            });
        mSortDirty = false;
    }

    // ---- 并行预计算阶段（纯CPU变换，不涉及OpenGL）----
    int total = static_cast<int>(mGameObjects.size());

    if (total >= 90) {
        mThreadPool->Dispatch(total, [this](int start, int end) {
            for (int i = start; i < end; i++) {
                if (mGameObjects[i]->IsActive())
                    mGameObjects[i]->PrepareForDraw();
            }
            });
    }
    else {
        for (size_t i = 0; i < mGameObjects.size(); i++) {
            auto* obj = mGameObjects[i].get();
            if (obj->IsActive()) {
                obj->PrepareForDraw();
            }
        }
    }

    // ---- 串行绘制阶段（按 mRenderOrder 顺序，保证绘制顺序正确）----
    for (size_t i = 0; i < mGameObjects.size(); ++i) {
        auto* obj = mGameObjects[i].get();
        if (obj->IsActive()) {
            obj->Draw(g);
        }
    }
}

std::vector<std::shared_ptr<GameObject>> GameObjectManager::FindGameObjectsWithTag(const std::string& tag) {
    std::vector<std::shared_ptr<GameObject>> result;
    for (auto& obj : mGameObjects) {
        if (obj->GetTag() == tag) {
            result.push_back(obj);
        }
    }
    return result;
}

std::shared_ptr<GameObject> GameObjectManager::FindGameObjectWithTag(const std::string& tag) {
    auto objects = FindGameObjectsWithTag(tag);
    return objects.empty() ? nullptr : objects[0];
}

void GameObjectManager::ClearAll() {
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

void GameObjectManager::PrintPoolStats() const {
    if (mBulletPool) {
        mBulletPool->PrintStats();
    }
}

void GameObjectManager::ResetAllLayers() {
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

void GameObjectManager::AssignRenderOrder(std::shared_ptr<GameObject> gameObject, RenderLayer layer) {
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

void GameObjectManager::RecycleRenderOrder(int renderOrder, RenderLayer layer, int key) {
    int subOrder = renderOrder - static_cast<int>(layer);
    if (subOrder >= 0 && subOrder < 10000) {  // 范围Χ 0~9999
        if (key >= 0) {
            mLayerKeyRecycledOrders[layer][key].insert(subOrder);
        }
        else {
            mLayerRecycledOrders[layer].insert(subOrder);
        }
    }
}

int GameObjectManager::GetNextSubOrder(RenderLayer layer) {
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

int GameObjectManager::GetNextSubOrderForKey(RenderLayer layer, int key) {
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
    ResetKeyLayer(layer, key);
    int globalSub = key * SUBORDER_PER_KEY;  // 从 0 开始
    mLayerKeyMaxLocalIdx[layer][key] = 1;    // 下一个可用本地索引为 1
    return globalSub;
}

void GameObjectManager::ResetKeyLayer(RenderLayer layer, int key) {
    mLayerKeyMaxLocalIdx[layer][key] = 0;
    mLayerKeyRecycledOrders[layer][key].clear();
}

void GameObjectManager::ResetLayer(RenderLayer layer) {
    mLayerMaxSubOrder[layer] = 0;
    mLayerRecycledOrders[layer].clear();
}
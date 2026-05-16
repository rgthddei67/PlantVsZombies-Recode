#include "GameObjectManager.h"
#include "../Profiler.h"

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

void GameObjectManager::DestroyGameObject(GameObject* raw) {
	if (!raw) return;
	for (const auto& obj : mGameObjects) {
		if (obj.get() == raw) {
			DestroyGameObject(obj);
			return;
		}
	}
	for (const auto& obj : mObjectsToAdd) {
		if (obj.get() == raw) {
			DestroyGameObject(obj);
			return;
		}
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
	//PROFILE_SCOPE("2.GOM_Update(serial)");
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
	// PROFILE_SCOPE("4.Draw_sort(serial)");
	if (mSortDirty) {
		std::sort(mGameObjects.begin(), mGameObjects.end(),
			[](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
				return a->GetRenderOrder() < b->GetRenderOrder();
			});
		mSortDirty = false;
	}

	const int total = static_cast<int>(mGameObjects.size());

	// ---- 并行预计算阶段（纯CPU变换，不涉及OpenGL）----
	// PROFILE_SCOPE("5.Draw_prepare(par)");
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

	// ---- 绘制阶段：小对象数走原串行；大场景走并行 record + 主线程 replay ----
	constexpr int kParallelDrawThreshold = 200;  // 小于此阈值，并行 dispatch overhead 不划算

	if (!g->IsParallelDrawEnabled() || total < kParallelDrawThreshold) {
		// 串行 fallback（菜单/图鉴等少对象场景，行为与原代码完全等价）
		// PROFILE_SCOPE("6.Draw_submit(serial-fallback)");
		for (size_t i = 0; i < mGameObjects.size(); ++i) {
			auto* obj = mGameObjects[i].get();
			if (obj->IsActive()) {
				const bool clipped = obj->HasClipRect();
				if (clipped) {
					const auto& cr = obj->GetClipRect();
					g->PushClipRect(cr.x, cr.y, cr.w, cr.h);
				}
				obj->Draw(g);
				if (clipped) {
					g->PopClipRect();
				}
			}
		}
		return;
	}

	// 并行 record + replay
	// workers 数和 ThreadPool 内部 numActive 必须保持一致，否则 slot 索引推导（start/chunkSize）
	// 可能错位。ThreadPool::Dispatch 把 numActive 钳到 min(线程数, total)，我们这里同步钳一下。
	int numWorkers = static_cast<int>(std::thread::hardware_concurrency());
	if (numWorkers < 1) numWorkers = 1;
	if (numWorkers > total) numWorkers = total;

	// PROFILE_SCOPE("6.Draw_submit(par-record)");
	g->BeginParallelRecord(numWorkers);

	mThreadPool->Dispatch(total, [this, g, total, numWorkers](int start, int end) {
		const int chunkSize = (total + numWorkers - 1) / numWorkers;
		const int slot = (chunkSize > 0) ? (start / chunkSize) : 0;

		g->SetWorkerSlot(slot);
		for (int i = start; i < end; ++i) {
			auto* obj = mGameObjects[i].get();
			if (!obj->IsActive()) continue;
			const bool clipped = obj->HasClipRect();
			if (clipped) {
				const auto& cr = obj->GetClipRect();
				g->PushClipRect(cr.x, cr.y, cr.w, cr.h);
			}
			obj->Draw(g);
			if (clipped) {
				g->PopClipRect();
			}
		}
		g->ClearWorkerSlot();
		});

	// PROFILE_SCOPE("7.Draw_replay(serial)");
	g->ReplayAndEndParallel();
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

void GameObjectManager::AssignRenderOrder(GameObject* gameObject, RenderLayer layer) {
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
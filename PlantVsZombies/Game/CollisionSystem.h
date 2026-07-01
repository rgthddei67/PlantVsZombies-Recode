#pragma once
#ifndef _COLLISION_SYSTEM_H
#define _COLLISION_SYSTEM_H

#include "ColliderComponent.h"
#include "ThreadPool.h"
#include "../Profiler.h"   // 诊断：sweep 迭代/拒绝计数上报（-Profile 时才累加）
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cstdint>

class CollisionSystem {
private:
	std::vector<ColliderComponent*> colliders;
	std::unordered_set<uint64_t> currentCollisions;

	std::unique_ptr<ThreadPool> mThreadPool;
	static constexpr int PARALLEL_THRESHOLD = 100;
	// PvZ 默认 5 行，泳池 6 行，留余地到 8（屋顶/将来扩展）
	static constexpr int MAX_ROWS = 8;
	uint32_t mNextColliderID = 1;

	struct DetectedPair {
		ColliderComponent* a;
		ColliderComponent* b;
		uint64_t pairKey;
	};

	// 跨帧复用的临时容器：每帧 clear() 而非重新分配
	std::vector<ColliderComponent*> mActiveColliders;
	std::array<std::vector<ColliderComponent*>, MAX_ROWS> mRowBuckets;
	std::array<std::vector<ColliderComponent*>, MAX_ROWS> mStaticRowBuckets;
	// seeker/target 拆分：僵尸(被动目标) 与 其余动态(seeker：子弹/割草机…) 分开存，跨帧复用。
	std::array<std::vector<ColliderComponent*>, MAX_ROWS> mRowZombies;
	std::array<std::vector<ColliderComponent*>, MAX_ROWS> mRowOthers;
	std::array<float, MAX_ROWS> mRowMaxZombieW{};   // 每行最大僵尸 AABB 宽，供二分下界
	std::vector<ColliderComponent*> mNoRowDynamic;
	std::vector<ColliderComponent*> mNoRowStatic;
	std::array<std::vector<DetectedPair>, MAX_ROWS> mRowResults;
	std::vector<DetectedPair> mNoRowResults;
	std::vector<int> mActiveRowIndices;
	std::unordered_set<uint64_t> mNewCollisions;

	// 诊断探针（仅 -Profile 时累加）：每行 sweep 内层迭代/拒绝/检测/命中计数。
	// detectRow 只写自己那行的槽位 → 并行无数据竞争；派发结束后主线程汇总一次上报 Profiler。
	std::array<uint64_t, MAX_ROWS> mRowIters{};
	std::array<uint64_t, MAX_ROWS> mRowRejects{};
	std::array<uint64_t, MAX_ROWS> mRowChecks{};
	std::array<uint64_t, MAX_ROWS> mRowHits{};

	static uint64_t MakePairKey(uint32_t idA, uint32_t idB) {
		if (idA > idB) std::swap(idA, idB);
		return (static_cast<uint64_t>(idA) << 32) | idB;
	}

	static bool CanCollide(const ColliderComponent* a, const ColliderComponent* b) {
		return ((a->layerMask & b->collisionMask) | (b->layerMask & a->collisionMask)) != 0;
	}

	CollisionSystem() {
		int n = std::max(1, (int)std::thread::hardware_concurrency());
		mThreadPool = std::make_unique<ThreadPool>(n);
	}

public:
	static CollisionSystem& GetInstance() {
		static CollisionSystem instance;
		return instance;
	}

	// 注册碰撞体
	void RegisterCollider(ColliderComponent* collider) {
		if (collider && !collider->mRegistered) {
			collider->colliderID = mNextColliderID++;
			collider->mRegistered = true;
			colliders.push_back(collider);
		}
	}

	// 注销碰撞体
	void UnregisterCollider(ColliderComponent* collider) {
		auto it = std::find(colliders.begin(), colliders.end(), collider);
		if (it != colliders.end()) {
			uint32_t id = collider->colliderID;
			std::vector<uint64_t> toRemove;
			for (auto key : currentCollisions) {
				uint32_t hi = static_cast<uint32_t>(key >> 32);
				uint32_t lo = static_cast<uint32_t>(key & 0xFFFFFFFF);
				if (hi == id || lo == id) {
					toRemove.push_back(key);
				}
			}
			for (auto key : toRemove) {
				currentCollisions.erase(key);
			}
			// 触发碰撞退出回调
			for (auto* other : colliders) {
				if (other == collider) continue;
				uint64_t pairKey = MakePairKey(id, other->colliderID);
				for (auto removedKey : toRemove) {
					if (removedKey == pairKey) {
						HandleCollisionExit(collider, other);
						break;
					}
				}
			}

			collider->mRegistered = false;
			colliders.erase(it);
		}
	}

	void Update() {
		int totalColliders = (int)colliders.size();

		// 清空跨帧复用容器（capacity 保留）
		mActiveColliders.clear();
		for (auto& v : mRowBuckets)       v.clear();
		for (auto& v : mStaticRowBuckets) v.clear();
		for (auto& v : mRowZombies)       v.clear();
		for (auto& v : mRowOthers)        v.clear();
		mRowMaxZombieW.fill(0.0f);
		mNoRowDynamic.clear();
		mNoRowStatic.clear();
		for (auto& v : mRowResults)       v.clear();
		mNoRowResults.clear();
		mActiveRowIndices.clear();
		mNewCollisions.clear();
		mRowIters.fill(0);
		mRowRejects.fill(0);
		mRowChecks.fill(0);
		mRowHits.fill(0);

		// ── 阶段1: 缓存世界坐标和AABB + 构建活跃列表 ──
		mActiveColliders.reserve(totalColliders);

		if (totalColliders >= PARALLEL_THRESHOLD && mThreadPool) {
			mThreadPool->Dispatch(totalColliders, [this](int start, int end) {
				for (int i = start; i < end; i++) {
					auto* col = colliders[i];
					auto* gameObj = col->GetGameObject();
					if (!col->mEnabled || !gameObj || !gameObj->IsActive()) continue;
					col->cachedWorldPos = col->GetWorldPosition();
					col->cachedBounds = col->GetBoundingBox();
				}
				});
			for (auto* col : colliders) {
				if (!col->mEnabled) continue;
				auto* gameObj = col->GetGameObject();
				if (!gameObj || !gameObj->IsActive()) continue;
				mActiveColliders.push_back(col);
			}
		}
		else {
			for (auto* col : colliders) {
				auto* gameObj = col->GetGameObject();
				if (!col->mEnabled || !gameObj || !gameObj->IsActive()) continue;
				col->cachedWorldPos = col->GetWorldPosition();
				col->cachedBounds = col->GetBoundingBox();
				mActiveColliders.push_back(col);
			}
		}

		// ── 阶段2: 分桶（array 直接索引，避免 unordered_map 哈希开销） ──
		for (auto* col : mActiveColliders) {
			if (col->layerMask == CollisionLayer::NONE && col->collisionMask == CollisionLayer::NONE)
				continue;
			int row = col->GetGameObject()->GetSortingKey();
			bool inRange = (row >= 0 && row < MAX_ROWS);
			if (col->isStatic) {
				if (inRange) mStaticRowBuckets[row].push_back(col);
				else         mNoRowStatic.push_back(col);
			}
			else {
				if (inRange) {
					mRowBuckets[row].push_back(col);   // TODO(Task3): 双写过渡，Task3 删除
					if (col->layerMask == CollisionLayer::ZOMBIE) {
						mRowZombies[row].push_back(col);
						const float w = col->cachedBounds.w;
						if (w > mRowMaxZombieW[row]) mRowMaxZombieW[row] = w;
					}
					else {
						mRowOthers[row].push_back(col);
					}
				}
				else {
					mNoRowDynamic.push_back(col);
				}
			}
		}

		// ── 阶段3: 检测（sweep-and-prune + 层掩码） ──
		int totalDynamic = 0;
		for (int r = 0; r < MAX_ROWS; r++) {
			if (!mRowBuckets[r].empty()) {
				mActiveRowIndices.push_back(r);
				totalDynamic += (int)mRowBuckets[r].size();
			}
		}
		int numRows = (int)mActiveRowIndices.size();

		auto detectRow = [this](int row) {
			auto& bucket = mRowBuckets[row];
			auto& results = mRowResults[row];
			const bool prof = g_ProfileEnabled;   // 诊断：整行只读一次，内层分支高度可预测
			uint64_t nIter = 0, nReject = 0, nCheck = 0, nHit = 0;

			std::sort(bucket.begin(), bucket.end(),
				[](const ColliderComponent* a, const ColliderComponent* b) {
					return a->cachedBounds.x < b->cachedBounds.x;
				});

			for (size_t i = 0; i < bucket.size(); ++i) {
				float rightEdge = bucket[i]->cachedBounds.x + bucket[i]->cachedBounds.w;
				for (size_t j = i + 1; j < bucket.size(); ++j) {
					if (bucket[j]->cachedBounds.x > rightEdge) break;
					if (prof) ++nIter;
					if (!CanCollide(bucket[i], bucket[j])) { if (prof) ++nReject; continue; }
					if (prof) ++nCheck;
					if (CheckCollision(bucket[i], bucket[j])) {
						auto* a = bucket[i]; auto* b = bucket[j];
						uint64_t key = MakePairKey(a->colliderID, b->colliderID);
						results.push_back({ a, b, key });
						if (prof) ++nHit;
					}
				}
			}

			auto& staticBucket = mStaticRowBuckets[row];
			if (!staticBucket.empty()) {
				for (auto* d : bucket) {
					for (auto* s : staticBucket) {
						if (!CanCollide(d, s)) continue;
						if (CheckCollision(d, s)) {
							uint64_t key = MakePairKey(d->colliderID, s->colliderID);
							results.push_back({ d, s, key });
						}
					}
				}
			}

			for (auto* d : bucket) {
				for (auto* s : mNoRowStatic) {
					if (!CanCollide(d, s)) continue;
					if (CheckCollision(d, s)) {
						uint64_t key = MakePairKey(d->colliderID, s->colliderID);
						results.push_back({ d, s, key });
					}
				}
			}

			if (prof) {
				mRowIters[row]   = nIter;
				mRowRejects[row] = nReject;
				mRowChecks[row]  = nCheck;
				mRowHits[row]    = nHit;
			}
			};

		if (numRows > 1 && totalDynamic >= PARALLEL_THRESHOLD && mThreadPool) {
			mThreadPool->Dispatch(numRows, [this, &detectRow](int start, int end) {
				for (int ri = start; ri < end; ri++) detectRow(mActiveRowIndices[ri]);
				});
		}
		else {
			for (int ri = 0; ri < numRows; ri++) detectRow(mActiveRowIndices[ri]);
		}

		// 诊断：并行派发已结束（隐式屏障），主线程安全汇总各行 sweep 计数上报 Profiler。
		if (g_ProfileEnabled) {
			uint64_t sIter = 0, sReject = 0, sCheck = 0, sHit = 0;
			for (int ri = 0; ri < numRows; ri++) {
				int r = mActiveRowIndices[ri];
				sIter += mRowIters[r];
				sReject += mRowRejects[r];
				sCheck += mRowChecks[r];
				sHit += mRowHits[r];
			}
			Profiler::Get().CountSweep(sIter, sReject, sCheck, sHit);
		}

		// noRowDynamic 串行检测
		for (size_t i = 0; i < mNoRowDynamic.size(); ++i) {
			auto* a = mNoRowDynamic[i];
			for (size_t j = i + 1; j < mNoRowDynamic.size(); ++j) {
				auto* b = mNoRowDynamic[j];
				if (!CanCollide(a, b)) continue;
				if (CheckCollision(a, b)) {
					uint64_t key = MakePairKey(a->colliderID, b->colliderID);
					mNoRowResults.push_back({ a, b, key });
				}
			}
			for (auto& bucket : mRowBuckets) {
				for (auto* b : bucket) {
					if (!CanCollide(a, b)) continue;
					if (CheckCollision(a, b)) {
						uint64_t key = MakePairKey(a->colliderID, b->colliderID);
						mNoRowResults.push_back({ a, b, key });
					}
				}
			}
		}
		for (auto* d : mNoRowDynamic) {
			for (auto* s : mNoRowStatic) {
				if (!CanCollide(d, s)) continue;
				if (CheckCollision(d, s)) {
					uint64_t key = MakePairKey(d->colliderID, s->colliderID);
					mNoRowResults.push_back({ d, s, key });
				}
			}
		}

		// ── 阶段4: 回调（主线程，无原子操作） ──
		for (auto& results : mRowResults) {
			for (auto& p : results) {
				mNewCollisions.insert(p.pairKey);
				HandleNewCollision(p.a, p.b, p.pairKey);
			}
		}
		for (auto& p : mNoRowResults) {
			mNewCollisions.insert(p.pairKey);
			HandleNewCollision(p.a, p.b, p.pairKey);
		}

		DetectEndedCollisions(mNewCollisions);
	}

	// 射线检测
	ColliderComponent* Raycast(const Vector& start, const Vector& end, const std::string& tag = "") {
		float maxDistance = Vector::distance(start, end);
		if (maxDistance == 0.0f) return nullptr;
		Vector direction = (end - start).normalized();

		ColliderComponent* closestHit = nullptr;
		float closestDistance = maxDistance;

		for (auto* collider : colliders) {
			if (!collider->mEnabled || !collider->GetGameObject()->IsActive()) continue;
			if (!tag.empty() && collider->GetGameObject()->GetTag() != tag) continue;

			SDL_FRect bounds = collider->GetBoundingBox();

			// 射线与AABB碰撞检测
			float t1 = (bounds.x - start.x) / direction.x;
			float t2 = (bounds.x + bounds.w - start.x) / direction.x;
			float t3 = (bounds.y - start.y) / direction.y;
			float t4 = (bounds.y + bounds.h - start.y) / direction.y;

			float tmin = std::max(std::min(t1, t2), std::min(t3, t4));
			float tmax = std::min(std::max(t1, t2), std::max(t3, t4));

			if (tmax >= 0 && tmin <= tmax && tmin <= maxDistance) {
				if (tmin < closestDistance) {
					closestDistance = tmin;
					closestHit = collider;
				}
			}
		}

		return closestHit;
	}

	// 在一定区域内查找碰撞体
	std::vector<ColliderComponent*> OverlapArea(const SDL_FRect& area, const std::string& tag = "") {
		std::vector<ColliderComponent*> results;

		for (auto* collider : colliders) {
			if (!collider->mEnabled || !collider->GetGameObject()->IsActive()) continue;
			if (!tag.empty() && collider->GetGameObject()->GetTag() != tag) continue;

			SDL_FRect bounds = collider->GetBoundingBox();
			if (CheckRectCollision(area, bounds)) {
				results.push_back(collider);
			}
		}

		return results;
	}

	// 清空所有碰撞体
	void ClearAll() {
		for (auto* col : colliders) col->mRegistered = false;
		colliders.clear();
		currentCollisions.clear();
		mNextColliderID = 1;
	}

private:
	bool CheckCollision(const ColliderComponent* a, const ColliderComponent* b) {
		const SDL_FRect& rectA = a->cachedBounds;
		const SDL_FRect& rectB = b->cachedBounds;

		if (!CheckRectCollision(rectA, rectB)) {
			return false;
		}

		if (a->colliderType == ColliderType::CIRCLE && b->colliderType == ColliderType::CIRCLE) {
			float radiusA = a->size.x * 0.5f;
			float radiusB = b->size.x * 0.5f;
			float sumR = radiusA + radiusB;
			return Vector::sqrDistance(a->cachedWorldPos, b->cachedWorldPos) <= sumR * sumR;
		}
		else if (a->colliderType == ColliderType::CIRCLE && b->colliderType == ColliderType::BOX) {
			return CheckCircleRectCollision(a->cachedWorldPos, a->size.x * 0.5f, rectB);
		}
		else if (a->colliderType == ColliderType::BOX && b->colliderType == ColliderType::CIRCLE) {
			return CheckCircleRectCollision(b->cachedWorldPos, b->size.x * 0.5f, rectA);
		}
		else {
			return CheckRectCollision(rectA, rectB);
		}
	}

	// 矩形碰撞检测
	bool CheckRectCollision(const SDL_FRect& a, const SDL_FRect& b) const {
		return (a.x < b.x + b.w &&
			a.x + a.w > b.x &&
			a.y < b.y + b.h &&
			a.y + a.h > b.y);
	}

	// 圆形与矩形碰撞检测
	bool CheckCircleRectCollision(const Vector& circleCenter, float radius, const SDL_FRect& rect) const {
		float closestX = std::max(rect.x, std::min(circleCenter.x, rect.x + rect.w));
		float closestY = std::max(rect.y, std::min(circleCenter.y, rect.y + rect.h));

		float distanceX = circleCenter.x - closestX;
		float distanceY = circleCenter.y - closestY;

		return (distanceX * distanceX + distanceY * distanceY) <= (radius * radius);
	}

	// 处理碰撞开始
	void HandleCollisionEnter(ColliderComponent* a, ColliderComponent* b) {
		if (a->isTrigger && a->onTriggerEnter) {
			a->onTriggerEnter(b);
		}
		else if (a->onCollisionEnter) {
			a->onCollisionEnter(b);
		}

		if (b->isTrigger && b->onTriggerEnter) {
			b->onTriggerEnter(a);
		}
		else if (b->onCollisionEnter) {
			b->onCollisionEnter(a);
		}
	}

	// 处理碰撞结束
	void HandleCollisionExit(ColliderComponent* a, ColliderComponent* b) {
		if (a->isTrigger && a->onTriggerExit) {
			a->onTriggerExit(b);
		}
		else if (a->onCollisionExit) {
			a->onCollisionExit(b);
		}

		if (b->isTrigger && b->onTriggerExit) {
			b->onTriggerExit(a);
		}
		else if (b->onCollisionExit) {
			b->onCollisionExit(a);
		}
	}

	// 处理新碰撞
	void HandleNewCollision(ColliderComponent* a, ColliderComponent* b, uint64_t pairKey) {
		if (currentCollisions.find(pairKey) == currentCollisions.end()) {
			HandleCollisionEnter(a, b);
			currentCollisions.insert(pairKey);
		}
		else {
			if (a->isTrigger && a->onTriggerStay) {
				a->onTriggerStay(b);
			}
			if (b->isTrigger && b->onTriggerStay) {
				b->onTriggerStay(a);
			}
		}
	}

	void DetectEndedCollisions(const std::unordered_set<uint64_t>& newCollisions) {
		std::vector<uint64_t> endedKeys;

		for (auto key : currentCollisions) {
			if (newCollisions.count(key) == 0) {
				endedKeys.push_back(key);
			}
		}

		if (endedKeys.empty()) return;

		// 用 ID 索引建立 raw 指针映射，避免每次扫描整个 colliders 列表
		std::unordered_map<uint32_t, ColliderComponent*> idMap;
		idMap.reserve(colliders.size());
		for (auto* col : colliders) idMap[col->colliderID] = col;

		for (auto key : endedKeys) {
			uint32_t idA = static_cast<uint32_t>(key >> 32);
			uint32_t idB = static_cast<uint32_t>(key & 0xFFFFFFFF);
			auto itA = idMap.find(idA);
			auto itB = idMap.find(idB);
			if (itA != idMap.end() && itB != idMap.end())
				HandleCollisionExit(itA->second, itB->second);
			currentCollisions.erase(key);
		}
	}
};

#endif

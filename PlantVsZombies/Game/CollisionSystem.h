#pragma once
#ifndef _COLLISION_SYSTEM_H
#define _COLLISION_SYSTEM_H

#include "ColliderComponent.h"
#include "ThreadPool.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstdint>

class CollisionSystem {
private:
    std::vector<std::shared_ptr<ColliderComponent>> colliders;
    std::unordered_set<uint64_t> currentCollisions;

    std::unique_ptr<ThreadPool> mThreadPool;
    static constexpr int PARALLEL_THRESHOLD = 100;
    uint32_t mNextColliderID = 1;

    struct DetectedPair {
        ColliderComponent* a;
        ColliderComponent* b;
        uint64_t pairKey;
    };

    static uint64_t MakePairKey(uint32_t idA, uint32_t idB) {
        if (idA > idB) std::swap(idA, idB);
        return (static_cast<uint64_t>(idA) << 32) | idB;
    }

    static bool CanCollide(const ColliderComponent* a, const ColliderComponent* b) {
        return (a->layerMask & b->collisionMask) | (b->layerMask & a->collisionMask);
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
    void RegisterCollider(std::shared_ptr<ColliderComponent> collider) {
        if (collider && !collider->mRegistered) {
            collider->colliderID = mNextColliderID++;
            collider->mRegistered = true;
            colliders.push_back(collider);
        }
    }

    // 注销碰撞体
    void UnregisterCollider(std::shared_ptr<ColliderComponent> collider) {
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
            for (auto& other : colliders) {
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

        // ── 阶段1: 缓存世界坐标和AABB + 构建活跃列表 ──
        std::vector<ColliderComponent*> activeColliders;
        activeColliders.reserve(totalColliders);

        if (totalColliders >= PARALLEL_THRESHOLD && mThreadPool) {
            mThreadPool->Dispatch(totalColliders, [this](int start, int end) {
                for (int i = start; i < end; i++) {
                    auto& col = colliders[i];
                    auto gameObj = col->GetGameObject();
                    if (!col->mEnabled || !gameObj || !gameObj->IsActive()) continue;
                    col->cachedWorldPos = col->GetWorldPosition();
                    col->cachedBounds   = col->GetBoundingBox();
                }
            });
            for (auto& col : colliders) {
                if (!col->mEnabled) continue;
                auto gameObj = col->GetGameObject();
                if (!gameObj || !gameObj->IsActive()) continue;
                activeColliders.push_back(col.get());
            }
        } else {
            for (auto& col : colliders) {
                auto gameObj = col->GetGameObject();
                if (!col->mEnabled || !gameObj || !gameObj->IsActive()) continue;
                col->cachedWorldPos = col->GetWorldPosition();
                col->cachedBounds   = col->GetBoundingBox();
                activeColliders.push_back(col.get());
            }
        }

        // ── 阶段2: 分桶（raw pointer，无原子操作） ──
        std::unordered_map<int, std::vector<ColliderComponent*>> rowBuckets;
        std::vector<ColliderComponent*> noRowDynamic;
        std::unordered_map<int, std::vector<ColliderComponent*>> staticRowBuckets;
        std::vector<ColliderComponent*> noRowStatic;

        for (auto* col : activeColliders) {
            if (col->layerMask == CollisionLayer::NONE && col->collisionMask == CollisionLayer::NONE)
                continue;
            int row = col->GetGameObject()->GetSortingKey();
            if (col->isStatic) {
                if (row >= 0) staticRowBuckets[row].push_back(col);
                else          noRowStatic.push_back(col);
            } else {
                if (row >= 0) rowBuckets[row].push_back(col);
                else          noRowDynamic.push_back(col);
            }
        }

        // ── 阶段3: 检测（sweep-and-prune + 层掩码 + raw pointer） ──
        std::vector<int> rowKeys;
        rowKeys.reserve(rowBuckets.size());
        for (auto& kv : rowBuckets) rowKeys.push_back(kv.first);
        int numRows = (int)rowKeys.size();

        std::vector<std::vector<DetectedPair>> rowResults(numRows);
        int totalDynamic = 0;
        for (auto& kv : rowBuckets) totalDynamic += (int)kv.second.size();

        auto detectRow = [&](int ri) {
            auto& bucket = rowBuckets[rowKeys[ri]];
            auto& results = rowResults[ri];

            std::sort(bucket.begin(), bucket.end(),
                [](const ColliderComponent* a, const ColliderComponent* b) {
                    return a->cachedBounds.x < b->cachedBounds.x;
                });

            for (size_t i = 0; i < bucket.size(); ++i) {
                float rightEdge = bucket[i]->cachedBounds.x + bucket[i]->cachedBounds.w;
                for (size_t j = i + 1; j < bucket.size(); ++j) {
                    if (bucket[j]->cachedBounds.x > rightEdge) break;
                    if (!CanCollide(bucket[i], bucket[j])) continue;
                    if (CheckCollision(bucket[i], bucket[j])) {
                        auto* a = bucket[i]; auto* b = bucket[j];
                        uint64_t key = MakePairKey(a->colliderID, b->colliderID);
                        results.push_back({ a, b, key });
                    }
                }
            }

            auto sit = staticRowBuckets.find(rowKeys[ri]);
            if (sit != staticRowBuckets.end()) {
                for (auto* d : bucket) {
                    for (auto* s : sit->second) {
                        if (!CanCollide(d, s)) continue;
                        if (CheckCollision(d, s)) {
                            uint64_t key = MakePairKey(d->colliderID, s->colliderID);
                            results.push_back({ d, s, key });
                        }
                    }
                }
            }

            for (auto* d : bucket) {
                for (auto* s : noRowStatic) {
                    if (!CanCollide(d, s)) continue;
                    if (CheckCollision(d, s)) {
                        uint64_t key = MakePairKey(d->colliderID, s->colliderID);
                        results.push_back({ d, s, key });
                    }
                }
            }
        };

        if (numRows > 1 && totalDynamic >= PARALLEL_THRESHOLD && mThreadPool) {
            mThreadPool->Dispatch(numRows, [&](int start, int end) {
                for (int ri = start; ri < end; ri++) detectRow(ri);
            });
        } else {
            for (int ri = 0; ri < numRows; ri++) detectRow(ri);
        }

        // noRowDynamic 串行检测
        std::vector<DetectedPair> noRowResults;
        for (size_t i = 0; i < noRowDynamic.size(); ++i) {
            auto* a = noRowDynamic[i];
            for (size_t j = i + 1; j < noRowDynamic.size(); ++j) {
                auto* b = noRowDynamic[j];
                if (!CanCollide(a, b)) continue;
                if (CheckCollision(a, b)) {
                    uint64_t key = MakePairKey(a->colliderID, b->colliderID);
                    noRowResults.push_back({ a, b, key });
                }
            }
            for (auto& kv : rowBuckets) {
                for (auto* b : kv.second) {
                    if (!CanCollide(a, b)) continue;
                    if (CheckCollision(a, b)) {
                        uint64_t key = MakePairKey(a->colliderID, b->colliderID);
                        noRowResults.push_back({ a, b, key });
                    }
                }
            }
        }
        for (auto* d : noRowDynamic) {
            for (auto* s : noRowStatic) {
                if (!CanCollide(d, s)) continue;
                if (CheckCollision(d, s)) {
                    uint64_t key = MakePairKey(d->colliderID, s->colliderID);
                    noRowResults.push_back({ d, s, key });
                }
            }
        }

        // ── 阶段4: 回调（主线程，shared_ptr 仅在此处使用） ──
        std::unordered_set<uint64_t> newCollisions;

        for (auto& results : rowResults) {
            for (auto& p : results) {
                newCollisions.insert(p.pairKey);
                HandleNewCollision(p.a, p.b, p.pairKey);
            }
        }
        for (auto& p : noRowResults) {
            newCollisions.insert(p.pairKey);
            HandleNewCollision(p.a, p.b, p.pairKey);
        }

        DetectEndedCollisions(newCollisions);
    }

    // 射线检测
    std::shared_ptr<ColliderComponent> Raycast(const Vector& start, const Vector& end, const std::string& tag = "") {
        Vector direction = (end - start).normalized();
        float maxDistance = Vector::distance(start, end);

        std::shared_ptr<ColliderComponent> closestHit = nullptr;
        float closestDistance = maxDistance;

        for (auto collider : colliders) {
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
    std::vector<std::shared_ptr<ColliderComponent>> OverlapArea(const SDL_FRect& area, const std::string& tag = "") {
        std::vector<std::shared_ptr<ColliderComponent>> results;

        for (auto collider : colliders) {
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
        for (auto& col : colliders) col->mRegistered = false;
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
    void HandleCollisionEnter(std::shared_ptr<ColliderComponent> a, std::shared_ptr<ColliderComponent> b) {
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
    void HandleCollisionExit(std::shared_ptr<ColliderComponent> a, std::shared_ptr<ColliderComponent> b) {
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

    std::shared_ptr<ColliderComponent> ToShared(ColliderComponent* raw) {
        return std::static_pointer_cast<ColliderComponent>(raw->shared_from_this());
    }

    // 处理新碰撞
    void HandleNewCollision(ColliderComponent* a, ColliderComponent* b, uint64_t pairKey) {
        if (currentCollisions.find(pairKey) == currentCollisions.end()) {
            auto asp = ToShared(a);
            auto bsp = ToShared(b);
            HandleCollisionEnter(asp, bsp);
            currentCollisions.insert(pairKey);
        }
        else {
            if (a->onTriggerStay) {
                auto bsp = ToShared(b);
                a->onTriggerStay(bsp);
            }
            if (b->onTriggerStay) {
                auto asp = ToShared(a);
                b->onTriggerStay(asp);
            }
        }
    }

    // 检测结束的碰撞
    void DetectEndedCollisions(const std::unordered_set<uint64_t>& newCollisions) {
        std::vector<uint64_t> endedKeys;

        for (auto key : currentCollisions) {
            if (newCollisions.count(key) == 0) {
                endedKeys.push_back(key);
            }
        }

        for (auto key : endedKeys) {
            uint32_t idA = static_cast<uint32_t>(key >> 32);
            uint32_t idB = static_cast<uint32_t>(key & 0xFFFFFFFF);
            std::shared_ptr<ColliderComponent> a, b;
            for (auto& col : colliders) {
                if (col->colliderID == idA) a = col;
                else if (col->colliderID == idB) b = col;
                if (a && b) break;
            }
            if (a && b) HandleCollisionExit(a, b);
            currentCollisions.erase(key);
        }
    }
};

#endif
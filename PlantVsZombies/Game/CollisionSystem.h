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

struct ColliderPairHash {
    size_t operator()(const std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>& p) const {
        auto hash1 = std::hash<std::shared_ptr<ColliderComponent>>{}(p.first);
        auto hash2 = std::hash<std::shared_ptr<ColliderComponent>>{}(p.second);

        if (p.first < p.second) {
            return hash1 ^ (hash2 << 1);
        }
        else {
            return hash2 ^ (hash1 << 1);
        }
    }
};

class CollisionSystem {
private:
    std::vector<std::shared_ptr<ColliderComponent>> colliders;
    std::unordered_set<
        std::pair<std::shared_ptr<ColliderComponent>,
        std::shared_ptr<ColliderComponent>>,
        ColliderPairHash> currentCollisions;

    std::unique_ptr<ThreadPool> mThreadPool;
    static constexpr int PARALLEL_THRESHOLD = 100;

    struct DetectedPair {
        std::shared_ptr<ColliderComponent> a;
        std::shared_ptr<ColliderComponent> b;
    };

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
        if (collider && std::find(colliders.begin(), colliders.end(), collider) == colliders.end()) {
            colliders.push_back(collider);
        }
    }

    // 注销碰撞体
    void UnregisterCollider(std::shared_ptr<ColliderComponent> collider) {
        auto it = std::find(colliders.begin(), colliders.end(), collider);
        if (it != colliders.end()) {
            // 移除相关的碰撞记录
            std::vector<std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>> toRemove;
            for (const auto& pair : currentCollisions) {
                if (pair.first == collider || pair.second == collider) {
                    toRemove.push_back(pair);
                }
            }
            for (const auto& pair : toRemove) {
                HandleCollisionExit(pair.first, pair.second);
                currentCollisions.erase(pair);
            }

            colliders.erase(it);
        }
    }

    void Update() {
        int totalColliders = (int)colliders.size();

        // ── 阶段1: 并行缓存世界坐标和AABB ──
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
        } else {
            for (auto& col : colliders) {
                auto gameObj = col->GetGameObject();
                if (!col->mEnabled || !gameObj || !gameObj->IsActive()) continue;
                col->cachedWorldPos = col->GetWorldPosition();
                col->cachedBounds   = col->GetBoundingBox();
            }
        }

        // ── 阶段2: 串行分桶 ──
        std::vector<std::shared_ptr<ColliderComponent>> dynamicColliders;
        std::vector<std::shared_ptr<ColliderComponent>> staticColliders;
        for (auto& col : colliders) {
            auto gameObj = col->GetGameObject();
            if (!col->mEnabled || !gameObj || !gameObj->IsActive()) continue;
            if (col->isStatic) staticColliders.push_back(col);
            else                dynamicColliders.push_back(col);
        }

        std::unordered_map<int, std::vector<std::shared_ptr<ColliderComponent>>> rowBuckets;
        std::vector<std::shared_ptr<ColliderComponent>> noRowDynamic;
        for (auto& col : dynamicColliders) {
            int row = col->GetGameObject()->GetSortingKey();
            if (row >= 0) rowBuckets[row].push_back(col);
            else          noRowDynamic.push_back(col);
        }

        std::unordered_map<int, std::vector<std::shared_ptr<ColliderComponent>>> staticRowBuckets;
        std::vector<std::shared_ptr<ColliderComponent>> noRowStatic;
        for (auto& col : staticColliders) {
            int row = col->GetGameObject()->GetSortingKey();
            if (row >= 0) staticRowBuckets[row].push_back(col);
            else          noRowStatic.push_back(col);
        }

        // ── 阶段3: 并行检测（每行独立，无共享写入） ──
        std::vector<int> rowKeys;
        rowKeys.reserve(rowBuckets.size());
        for (auto& kv : rowBuckets) rowKeys.push_back(kv.first);
        int numRows = (int)rowKeys.size();

        std::vector<std::vector<DetectedPair>> rowResults(numRows);
        int totalDynamic = (int)dynamicColliders.size();

        if (numRows > 1 && totalDynamic >= PARALLEL_THRESHOLD && mThreadPool) {
            mThreadPool->Dispatch(numRows, [&](int start, int end) {
                for (int ri = start; ri < end; ri++) {
                    auto& bucket = rowBuckets[rowKeys[ri]];
                    auto& results = rowResults[ri];

                    for (size_t i = 0; i < bucket.size(); ++i) {
                        for (size_t j = i + 1; j < bucket.size(); ++j) {
                            if (CheckCollision(bucket[i], bucket[j])) {
                                auto& a = bucket[i]; auto& b = bucket[j];
                                results.push_back({ (a < b) ? a : b, (a < b) ? b : a });
                            }
                        }
                    }

                    auto sit = staticRowBuckets.find(rowKeys[ri]);
                    if (sit != staticRowBuckets.end()) {
                        for (auto& d : bucket) {
                            for (auto& s : sit->second) {
                                if (CheckCollision(d, s))
                                    results.push_back({ (d < s) ? d : s, (d < s) ? s : d });
                            }
                        }
                    }

                    for (auto& d : bucket) {
                        for (auto& s : noRowStatic) {
                            if (CheckCollision(d, s))
                                results.push_back({ (d < s) ? d : s, (d < s) ? s : d });
                        }
                    }
                }
            });
        } else {
            for (int ri = 0; ri < numRows; ri++) {
                auto& bucket = rowBuckets[rowKeys[ri]];
                auto& results = rowResults[ri];
                for (size_t i = 0; i < bucket.size(); ++i) {
                    for (size_t j = i + 1; j < bucket.size(); ++j) {
                        if (CheckCollision(bucket[i], bucket[j])) {
                            auto& a = bucket[i]; auto& b = bucket[j];
                            results.push_back({ (a < b) ? a : b, (a < b) ? b : a });
                        }
                    }
                }
                auto sit = staticRowBuckets.find(rowKeys[ri]);
                if (sit != staticRowBuckets.end()) {
                    for (auto& d : bucket) {
                        for (auto& s : sit->second) {
                            if (CheckCollision(d, s))
                                results.push_back({ (d < s) ? d : s, (d < s) ? s : d });
                        }
                    }
                }
                for (auto& d : bucket) {
                    for (auto& s : noRowStatic) {
                        if (CheckCollision(d, s))
                            results.push_back({ (d < s) ? d : s, (d < s) ? s : d });
                    }
                }
            }
        }

        // noRowDynamic 串行检测（通常很少）
        std::vector<DetectedPair> noRowResults;
        for (size_t i = 0; i < noRowDynamic.size(); ++i) {
            auto& a = noRowDynamic[i];
            for (size_t j = i + 1; j < noRowDynamic.size(); ++j) {
                auto& b = noRowDynamic[j];
                if (CheckCollision(a, b))
                    noRowResults.push_back({ (a < b) ? a : b, (a < b) ? b : a });
            }
            for (auto& kv : rowBuckets) {
                for (auto& b : kv.second) {
                    if (CheckCollision(a, b))
                        noRowResults.push_back({ (a < b) ? a : b, (a < b) ? b : a });
                }
            }
        }
        for (auto& d : noRowDynamic) {
            for (auto& s : staticColliders) {
                if (CheckCollision(d, s))
                    noRowResults.push_back({ (d < s) ? d : s, (d < s) ? s : d });
            }
        }

        // ── 阶段4: 串行回调（主线程，保证线程安全） ──
        std::unordered_set<
            std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>,
            ColliderPairHash> newCollisions;

        for (auto& results : rowResults) {
            for (auto& p : results) {
                auto collisionPair = std::make_pair(p.a, p.b);
                newCollisions.insert(collisionPair);
                HandleNewCollision(p.a, p.b, collisionPair);
            }
        }
        for (auto& p : noRowResults) {
            auto collisionPair = std::make_pair(p.a, p.b);
            newCollisions.insert(collisionPair);
            HandleNewCollision(p.a, p.b, collisionPair);
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
        colliders.clear();
        currentCollisions.clear();
    }

private:
    // 检查两个碰撞体是否发生碰撞
    // 读 cachedBounds / cachedWorldPos（由 Update 帧首写入），避免 Transform 哈希查找。
    bool CheckCollision(std::shared_ptr<ColliderComponent> a, std::shared_ptr<ColliderComponent> b) {
        const SDL_FRect& rectA = a->cachedBounds;
        const SDL_FRect& rectB = b->cachedBounds;

        // 粗略检查有无碰撞
        if (!CheckRectCollision(rectA, rectB)) {
            return false;
        }

        // 精确形状检查
        if (a->colliderType == ColliderType::CIRCLE && b->colliderType == ColliderType::CIRCLE) {
            float radiusA = a->size.x * 0.5f;
            float radiusB = b->size.x * 0.5f;
            float distance = Vector::distance(a->cachedWorldPos, b->cachedWorldPos);
            return distance <= (radiusA + radiusB);
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

    // 处理新碰撞
    void HandleNewCollision(std::shared_ptr<ColliderComponent> a,
        std::shared_ptr<ColliderComponent> b,
        const std::pair<std::shared_ptr<ColliderComponent>,
        std::shared_ptr<ColliderComponent>>&collisionPair) {
        // 如果是新碰撞
        if (currentCollisions.find(collisionPair) == currentCollisions.end()) {
            HandleCollisionEnter(a, b);
            currentCollisions.insert(collisionPair);
        }
        else {
            // 持续碰撞
            if (a->onTriggerStay) a->onTriggerStay(b);
            if (b->onTriggerStay) b->onTriggerStay(a);
        }
    }

    // 检测结束的碰撞
    void DetectEndedCollisions(const std::unordered_set<
        std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>,
        ColliderPairHash>& newCollisions) {
        std::vector<std::pair<std::shared_ptr<ColliderComponent>,
            std::shared_ptr<ColliderComponent>>> endedCollisions;

        for (const auto& collisionPair : currentCollisions) {
            if (newCollisions.count(collisionPair) == 0) {
                endedCollisions.push_back(collisionPair);
            }
        }

        for (const auto& collisionPair : endedCollisions) {
            HandleCollisionExit(collisionPair.first, collisionPair.second);
            currentCollisions.erase(collisionPair);
        }
    }
};

#endif
#pragma once
#ifndef _COLLISION_SYSTEM_H
#define _COLLISION_SYSTEM_H

#include "ColliderComponent.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <algorithm>

namespace std {
    template<>
    struct hash<std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>> {
        size_t operator()(const std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>& p) const {
            // 使用两个指针的地址组合来生成哈希
            auto hash1 = std::hash<std::shared_ptr<ColliderComponent>>{}(p.first);
            auto hash2 = std::hash<std::shared_ptr<ColliderComponent>>{}(p.second);

            // 组合哈希值，确保顺序不影响结果
            if (p.first < p.second) {
                return hash1 ^ (hash2 << 1);
            }
            else {
                return hash2 ^ (hash1 << 1);
            }
        }
    };
}

class CollisionSystem {
private:
    std::vector<std::shared_ptr<ColliderComponent>> colliders;  // 所有的collider组件
	std::unordered_set
        <std::pair<std::shared_ptr<ColliderComponent>, 
        std::shared_ptr<ColliderComponent>>> currentCollisions; // 所有碰撞记录

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
        std::vector<std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>> newCollisions;

        // 检测所有碰撞对
        for (size_t i = 0; i < colliders.size(); ++i) {
            auto colliderA = colliders[i];
            if (!colliderA->enabled || !colliderA->GetGameObject()->IsActive()) continue;

            for (size_t j = i + 1; j < colliders.size(); ++j) {
                auto colliderB = colliders[j];
                if (!colliderB->enabled || !colliderB->GetGameObject()->IsActive()) continue;

                if (CheckCollision(colliderA, colliderB)) {
                    auto collisionPair = (colliderA < colliderB) ?
                        std::make_pair(colliderA, colliderB) :
                        std::make_pair(colliderB, colliderA);

                    newCollisions.push_back(collisionPair);

                    // 如果是新碰撞
                    if (currentCollisions.find(collisionPair) == currentCollisions.end()) {
                        HandleCollisionEnter(colliderA, colliderB);
                        currentCollisions.insert(collisionPair);
                    }
                    else {
                        // 持续碰撞
                        if (colliderA->onTriggerStay) colliderA->onTriggerStay(colliderB);
                        if (colliderB->onTriggerStay) colliderB->onTriggerStay(colliderA);
                    }
                }
            }
        }

        // 检测碰撞结束
        std::vector<std::pair<std::shared_ptr<ColliderComponent>, std::shared_ptr<ColliderComponent>>> endedCollisions;
        for (const auto& collisionPair : currentCollisions) {
            if (std::find(newCollisions.begin(), newCollisions.end(), collisionPair) == newCollisions.end()) {
                endedCollisions.push_back(collisionPair);
            }
        }

        for (const auto& collisionPair : endedCollisions) {
            HandleCollisionExit(collisionPair.first, collisionPair.second);
            currentCollisions.erase(collisionPair);
        }
    }

    // 射线检测
    std::shared_ptr<ColliderComponent> Raycast(const Vector& start, const Vector& end, const std::string& tag = "") {
        Vector direction = (end - start).normalized();
        float maxDistance = Vector::distance(start, end);

        std::shared_ptr<ColliderComponent> closestHit = nullptr;
        float closestDistance = maxDistance;

        for (auto collider : colliders) {
            if (!collider->enabled || !collider->GetGameObject()->IsActive()) continue;
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
            if (!collider->enabled || !collider->GetGameObject()->IsActive()) continue;
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
    bool CheckCollision(std::shared_ptr<ColliderComponent> a, std::shared_ptr<ColliderComponent> b) {
        SDL_FRect rectA = a->GetBoundingBox();
        SDL_FRect rectB = b->GetBoundingBox();

        // 粗略检查有无碰撞
        if (!CheckRectCollision(rectA, rectB)) {
            return false;
        }

        // 精确形状检查
        if (a->colliderType == ColliderType::CIRCLE && b->colliderType == ColliderType::CIRCLE) {
            Vector posA = a->GetWorldPosition();
            Vector posB = b->GetWorldPosition();
            float radiusA = a->size.x * 0.5f;
            float radiusB = b->size.x * 0.5f;
            float distance = Vector::distance(posA, posB);
            return distance <= (radiusA + radiusB);
        }
        else if (a->colliderType == ColliderType::CIRCLE && b->colliderType == ColliderType::BOX) {
            return CheckCircleRectCollision(a->GetWorldPosition(), a->size.x * 0.5f, rectB);
        }
        else if (a->colliderType == ColliderType::BOX && b->colliderType == ColliderType::CIRCLE) {
            return CheckCircleRectCollision(b->GetWorldPosition(), b->size.x * 0.5f, rectA);
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
};

#endif
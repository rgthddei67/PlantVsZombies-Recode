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
            // ʹ������ָ��ĵ�ַ��������ɹ�ϣ
            auto hash1 = std::hash<std::shared_ptr<ColliderComponent>>{}(p.first);
            auto hash2 = std::hash<std::shared_ptr<ColliderComponent>>{}(p.second);

            // ��Ϲ�ϣֵ��ȷ��˳��Ӱ����
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
    std::vector<std::shared_ptr<ColliderComponent>> colliders;  // ���е�collider���
	std::unordered_set
        <std::pair<std::shared_ptr<ColliderComponent>, 
        std::shared_ptr<ColliderComponent>>> currentCollisions; // ������ײ��¼

public:
    static CollisionSystem& GetInstance() {
        static CollisionSystem instance;
        return instance;
    }

    // ע����ײ��
    void RegisterCollider(std::shared_ptr<ColliderComponent> collider) {
        if (collider && std::find(colliders.begin(), colliders.end(), collider) == colliders.end()) {
            colliders.push_back(collider);
        }
    }

    // ע����ײ��
    void UnregisterCollider(std::shared_ptr<ColliderComponent> collider) {
        auto it = std::find(colliders.begin(), colliders.end(), collider);
        if (it != colliders.end()) {
            // �Ƴ���ص���ײ��¼
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

        // ���������ײ��
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

                    // ���������ײ
                    if (currentCollisions.find(collisionPair) == currentCollisions.end()) {
                        HandleCollisionEnter(colliderA, colliderB);
                        currentCollisions.insert(collisionPair);
                    }
                    else {
                        // ������ײ
                        if (colliderA->onTriggerStay) colliderA->onTriggerStay(colliderB);
                        if (colliderB->onTriggerStay) colliderB->onTriggerStay(colliderA);
                    }
                }
            }
        }

        // �����ײ����
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

    // ���߼��
    std::shared_ptr<ColliderComponent> Raycast(const Vector& start, const Vector& end, const std::string& tag = "") {
        Vector direction = (end - start).normalized();
        float maxDistance = Vector::distance(start, end);

        std::shared_ptr<ColliderComponent> closestHit = nullptr;
        float closestDistance = maxDistance;

        for (auto collider : colliders) {
            if (!collider->enabled || !collider->GetGameObject()->IsActive()) continue;
            if (!tag.empty() && collider->GetGameObject()->GetTag() != tag) continue;

            SDL_FRect bounds = collider->GetBoundingBox();

            // ������AABB��ײ���
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

    // ��һ�������ڲ�����ײ��
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

    // ���������ײ��
    void ClearAll() {
        colliders.clear();
        currentCollisions.clear();
    }

private:
    // ���������ײ���Ƿ�����ײ
    bool CheckCollision(std::shared_ptr<ColliderComponent> a, std::shared_ptr<ColliderComponent> b) {
        SDL_FRect rectA = a->GetBoundingBox();
        SDL_FRect rectB = b->GetBoundingBox();

        // ���Լ��������ײ
        if (!CheckRectCollision(rectA, rectB)) {
            return false;
        }

        // ��ȷ��״���
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

    // ������ײ���
    bool CheckRectCollision(const SDL_FRect& a, const SDL_FRect& b) const {
        return (a.x < b.x + b.w &&
            a.x + a.w > b.x &&
            a.y < b.y + b.h &&
            a.y + a.h > b.y);
    }

    // Բ���������ײ���
    bool CheckCircleRectCollision(const Vector& circleCenter, float radius, const SDL_FRect& rect) const {
        float closestX = std::max(rect.x, std::min(circleCenter.x, rect.x + rect.w));
        float closestY = std::max(rect.y, std::min(circleCenter.y, rect.y + rect.h));

        float distanceX = circleCenter.x - closestX;
        float distanceY = circleCenter.y - closestY;

        return (distanceX * distanceX + distanceY * distanceY) <= (radius * radius);
    }

    // ������ײ��ʼ
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

    // ������ײ����
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
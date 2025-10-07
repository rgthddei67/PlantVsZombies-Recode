#pragma once
#ifndef _COLLIDER_COMPONENT_H
#define _COLLIDER_COMPONENT_H

#include "Component.h"
#include "TransformComponent.h"
#include "Definit.h"
#include <SDL.h>
#include <functional>
#include <memory>

// ��ײ��״����
enum class ColliderType {
    BOX,
    CIRCLE
};

class ColliderComponent : public Component {
public:
    Vector offset = Vector::zero();    // �������Ϸ�����ƫ��
    Vector size = Vector(50, 40);        // �ߴ磨����Ϊ��ߣ�Բ��Ϊֱ����
    ColliderType colliderType = ColliderType::BOX;
    bool isTrigger = false;            // �Ƿ��Ǵ�����
    bool isStatic = false;             // �Ƿ��Ǿ�̬��ײ��

    // ��ײ���¼����ص�������
    std::function<void(std::shared_ptr<ColliderComponent>)> onTriggerEnter;
    std::function<void(std::shared_ptr<ColliderComponent>)> onTriggerStay;
    std::function<void(std::shared_ptr<ColliderComponent>)> onTriggerExit;
    std::function<void(std::shared_ptr<ColliderComponent>)> onCollisionEnter;
    std::function<void(std::shared_ptr<ColliderComponent>)> onCollisionExit;

    ColliderComponent() = default;

    ColliderComponent(const Vector& size, ColliderType type = ColliderType::BOX)
        : size(size), colliderType(type) {
    }

    ColliderComponent(float width, float height)
        : size(width, height), colliderType(ColliderType::BOX) {
    }

    // ��ȡ��������λ��
    Vector GetWorldPosition() const {
        if (auto transform = GetTransform()) {
            return transform->position + offset;
        }
        return offset;
    }

    // ��ȡ�߽��
    SDL_FRect GetBoundingBox() const {
        Vector worldPos = GetWorldPosition();

        if (colliderType == ColliderType::CIRCLE) {
            float radius = size.x * 0.5f;
            return {
                worldPos.x - radius,
                worldPos.y - radius,
                size.x,
                size.y
            };
        }
        else {
            return {
                worldPos.x - size.x * 0.5f,
                worldPos.y - size.y * 0.5f,
                size.x,
                size.y
            };
        }
    }

	// �����Ƿ�����ײ����(������������ϵ��) Vector�汾
    bool ContainsPoint(const Vector& point) const {
        Vector worldPos = GetWorldPosition();

        switch (colliderType) {
        case ColliderType::BOX: {
            SDL_FRect rect = GetBoundingBox();
            return (point.x >= rect.x && point.x <= rect.x + rect.w &&
                point.y >= rect.y && point.y <= rect.y + rect.h);
        }

        case ColliderType::CIRCLE: {
            float radius = size.x * 0.5f;
            float distance = Vector::distance(worldPos, point);
            return distance <= radius;
        }
        }
        return false;
    }
	// �����Ƿ�����ײ����(������������ϵ��) float�汾
    bool ContainsPoint(float x, float y) const {
        return ContainsPoint(Vector(x, y));
	}

private:
    std::shared_ptr<TransformComponent> GetTransform() const {
        if (auto gameObj = GetGameObject()) 
        {  
            return gameObj->GetComponent<TransformComponent>();
        }
        return nullptr;
    }
};

#endif
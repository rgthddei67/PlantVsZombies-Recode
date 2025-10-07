#pragma once
#ifndef _COLLIDER_COMPONENT_H
#define _COLLIDER_COMPONENT_H

#include "Component.h"
#include "TransformComponent.h"
#include "Definit.h"
#include <SDL.h>
#include <functional>
#include <memory>

class TransformComponent;

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

    // ��ȡ����ռ�λ��
    Vector GetWorldPosition() const;

    // ��ȡ�߽��
    SDL_FRect GetBoundingBox() const;

    // �����Ƿ�����ײ����(��������ռ�����) Vector����
    bool ContainsPoint(const Vector& point) const;

    // �����Ƿ�����ײ����(��������ռ�����) float����
    bool ContainsPoint(float x, float y) const {
        return ContainsPoint(Vector(x, y));
    }

private:
    std::shared_ptr<TransformComponent> GetTransform() const;
};

#endif
#pragma once
#ifndef _TRANSFORM_COMPONENT_H
#define _TRANSFORM_COMPONENT_H

#include "Component.h"

class TransformComponent : public Component {
public:
    Vector position = Vector::zero();
    Vector scale = Vector::one();
    float rotation = 0.0f;  // ��ת�Ļ���

    TransformComponent() = default;
    TransformComponent(const Vector& pos) : position(pos) {}
    TransformComponent(float x, float y) : position(x, y) {}

    // �ƶ�x,y Vector�汾
    void Translate(const Vector& translation) {
        position += translation;
    }

    // �ƶ�x,y float�汾
    void Translate(float x, float y) {
        position.x += x;
        position.y += y;
    }

    // ��תangle��
    void Rotate(float angle) {
        rotation += angle;
    }

    // ����angle��
    void SetRotation(float angle) {
        rotation = angle;
    }

    // ����
    void Scale(const Vector& scaling) {
        scale.x *= scaling.x;
        scale.y *= scaling.y;
    }

    // ��������
    void SetScale(const Vector& scaling) {
        scale = scaling;
    }

    // ��ȡǰ������
    Vector GetForward() const {
        return Vector(cosf(rotation), sinf(rotation));
    }

    // ��ȡ�ұ�����
    Vector GetRight() const {
        return Vector(-sinf(rotation), cosf(rotation));
    }

	// ����λ�� Vector�汾
    void SetPosition(const Vector& newPos) {
        position = newPos;
    }

    // ����λ�� float�汾
    void SetPosition(float x, float y) {
        position.x = x;
        position.y = y;
    }

    // ��ȡ����λ�ã�����и�������Ҫ���㣬����򻯣�
    Vector GetWorldPosition() const {
        return position;
    }
};

#endif
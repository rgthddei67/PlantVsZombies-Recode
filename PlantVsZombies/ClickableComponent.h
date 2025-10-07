#pragma once
#ifndef _CLICKABLE_COMPONENT_H
#define _CLICKABLE_COMPONENT_H
#include "Component.h"
#include "ColliderComponent.h"
#include "InputHandler.h"
#include <iostream>
#include <functional>
#include <memory>

class ClickableComponent : public Component {
public:
	bool IsClickable = true;	// �ܷ���
	bool ConsumeEvent = true;   // �Ƿ����ĵ���¼����ڶ��UI�ص�֮ʱ��ֻ��һ���ܼ�⣩

    std::function<void()> onClick;
    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;
    std::function<void()> onMouseDown;
    std::function<void()> onMouseUp;

    //TODO: ���� ���캯���в�Ҫʹ��GetComponent ��Ϊ����û��ȫ����� ������Start��ʹ��
    ClickableComponent() = default;

    void Start() override {
        if (auto gameObject = this->GetGameObject())
        {
            collider = gameObject->GetComponent<ColliderComponent>();
            if (collider == nullptr)
            {
#ifdef _DEBUG
				std::cout << "ClickableComponent::Start:" 
                    << gameObject << "û��ColliderComponent������Զ����" << std::endl;
#endif
                collider = gameObject->AddComponent<ColliderComponent>(Vector(50, 50));
                collider->isTrigger = true;
            }
            else
            {
                collider->isTrigger = true;
            }
        }
	}

    void Update() override {
        eventConsumed = false;

        if (!IsClickable || !collider || !collider->enabled || eventConsumed) {
            return;
        }
        auto input = InputHandler::GetInstance();
        Vector mousePos = input->GetMousePosition();

        bool isOver = collider->ContainsPoint(mousePos);

        // ����������/�뿪�¼�
        if (isOver && !mouseOver) {
            mouseOver = true;
            if (onMouseEnter) onMouseEnter();
        }
        else if (!isOver && mouseOver) {
            mouseOver = false;
            if (onMouseExit) onMouseExit();
            mouseDown = false; // ����뿪ʱ���ð���״̬
        }

        if (mouseOver) {
            // ��갴��
            if (input->IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
                mouseDown = true;
                if (onMouseDown) onMouseDown();
            }

            // ����ͷţ������ɣ�
            if (input->IsMouseButtonReleased(SDL_BUTTON_LEFT) && mouseDown) {
                mouseDown = false;
                if (onClick) onClick();
                if (onMouseUp) onMouseUp();

                // �����¼�
                if (ConsumeEvent) {
                    eventConsumed = true;
                }
            }
        }
        else {
            // ���ⲿ�ͷ���갴ťʱ����״̬
            if (input->IsMouseButtonReleased(SDL_BUTTON_LEFT)) {
                mouseDown = false;
            }
        }
    }

    // ���õ������
    void SetClickArea(const Vector& size) {
        if (collider) {
            collider->size = size;
        }
    }

    // ���õ������ƫ��
    void SetClickOffset(const Vector& offset) {
        if (collider) {
            collider->offset = offset;
        }
    }

    // ����Ƿ��ɿ�
    bool IsMouseOver() const {
        return mouseOver;
    }

    // ����Ƿ���
    bool IsMouseDown() const {
        return mouseDown;
    }

private:
    std::shared_ptr<ColliderComponent> collider;
    bool mouseOver = false;
    bool mouseDown = false;
    bool eventConsumed = false;
};
#endif
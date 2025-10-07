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
	bool IsClickable = true;	// 能否点击
	bool ConsumeEvent = true;   // 是否消耗点击事件（在多个UI重叠之时，只有一个能检测）

    std::function<void()> onClick;
    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;
    std::function<void()> onMouseDown;
    std::function<void()> onMouseUp;

    //TODO: 警告 构造函数中不要使用GetComponent 因为对象还没完全构造好 可以在Start中使用
    ClickableComponent() = default;

    void Start() override {
        if (auto gameObject = this->GetGameObject())
        {
            collider = gameObject->GetComponent<ColliderComponent>();
            if (collider == nullptr)
            {
#ifdef _DEBUG
				std::cout << "ClickableComponent::Start:" 
                    << gameObject << "没有ColliderComponent组件！自动添加" << std::endl;
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

        // 处理鼠标进入/离开事件
        if (isOver && !mouseOver) {
            mouseOver = true;
            if (onMouseEnter) onMouseEnter();
        }
        else if (!isOver && mouseOver) {
            mouseOver = false;
            if (onMouseExit) onMouseExit();
            mouseDown = false; // 鼠标离开时重置按下状态
        }

        if (mouseOver) {
            // 鼠标按下
            if (input->IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
                mouseDown = true;
                if (onMouseDown) onMouseDown();
            }

            // 鼠标释放（点击完成）
            if (input->IsMouseButtonReleased(SDL_BUTTON_LEFT) && mouseDown) {
                mouseDown = false;
                if (onClick) onClick();
                if (onMouseUp) onMouseUp();

                // 消耗事件
                if (ConsumeEvent) {
                    eventConsumed = true;
                }
            }
        }
        else {
            // 在外部释放鼠标按钮时重置状态
            if (input->IsMouseButtonReleased(SDL_BUTTON_LEFT)) {
                mouseDown = false;
            }
        }
    }

    // 设置点击区域
    void SetClickArea(const Vector& size) {
        if (collider) {
            collider->size = size;
        }
    }

    // 设置点击区域偏移
    void SetClickOffset(const Vector& offset) {
        if (collider) {
            collider->offset = offset;
        }
    }

    // 鼠标是否松开
    bool IsMouseOver() const {
        return mouseOver;
    }

    // 鼠标是否按下
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
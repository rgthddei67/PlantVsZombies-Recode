#pragma once
#ifndef _CLICKABLE_COMPONENT_H
#define _CLICKABLE_COMPONENT_H

#include "Component.h"
#include "ColliderComponent.h"
#include <iostream>
#include <functional>
#include <memory>

class ClickableComponent : public Component {
public:
    bool IsClickable = true;    // �ܷ���
    bool ConsumeEvent = true;   // �Ƿ����ĵ���¼�

    std::function<void()> onClick;
    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;
    std::function<void()> onMouseDown;
    std::function<void()> onMouseUp;

    ClickableComponent() = default;

    void Start() override;
    void Update() override;

    void SetClickArea(const Vector& size);
    void SetClickOffset(const Vector& offset);
    bool IsMouseOver() const { return mouseOver; }
    bool IsMouseDown() const { return mouseDown; }

private:
    std::shared_ptr<ColliderComponent> collider;
    bool mouseOver = false;
    bool mouseDown = false;
    bool eventConsumed = false;
};

#endif
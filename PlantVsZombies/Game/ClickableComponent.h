#pragma once
#ifndef _CLICKABLE_COMPONENT_H
#define _CLICKABLE_COMPONENT_H

#include "Component.h"
#include <iostream>
#include <functional>
#include <memory>

class ColliderComponent;

class ClickableComponent : public Component {
public:
    bool IsClickable = true;    // 能否点击
    bool ConsumeEvent = true;   // 是否消耗点击事件

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
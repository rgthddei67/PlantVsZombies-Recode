#pragma once
#ifndef _CLICKABLE_COMPONENT_H
#define _CLICKABLE_COMPONENT_H

#include "Component.h"
#include <unordered_set>
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

    static void ClearConsumedEvents();
    static bool IsEventConsumedByHigherObject(const std::shared_ptr<GameObject>& currentObj,
        const std::vector<std::shared_ptr<GameObject>>& sortedObjects);

private:
    std::shared_ptr<ColliderComponent> collider;
    inline static std::unordered_set<ClickableComponent*> s_processedThisFrame;
    bool mouseOver = false;
    bool mouseDown = false;
    bool eventConsumed = false;
};

#endif
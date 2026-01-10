#pragma once
#ifndef _CLICKABLE_COMPONENT_H
#define _CLICKABLE_COMPONENT_H

#include "Component.h"
#include <unordered_set>
#include <functional>

class ColliderComponent;

class ClickableComponent : public Component {
public:
	bool IsClickable = true;    // 是否可点击
	bool ConsumeEvent = true;   // 是否消耗点击事件，阻止更低层对象响应
    bool ChangeCursorOnHover = true;   // 悬停时改变光标

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

    static void ProcessMouseEvents();
    static void ClearProcessedEvents();

private:
    std::shared_ptr<ColliderComponent> mCollider;

    // 存储当前帧处理过的点击事件
    inline static std::unordered_set<ClickableComponent*> s_processedEvents;

    // 鼠标状态
    bool mouseOver = false;
    bool mouseDown = false;
    bool prevMouseOver = false;

    // 当前帧是否有鼠标悬停在可点击对象上
    inline static bool s_hoveringClickable = false;
};

#endif
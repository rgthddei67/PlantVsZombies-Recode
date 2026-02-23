#pragma once
#ifndef _BUTTONMANAGER_H
#define _BUTTONMANAGER_H
#include "../UI/Button.h"
#include "../UI/InputHandler.h"
#include <vector>
#include <memory>

class ButtonManager
{
private:
    std::vector<std::shared_ptr<Button>> buttons; // 智能指针管理按钮

public:
    // 创建按钮并返回指针
    std::shared_ptr<Button> CreateButton(Vector pos = Vector::zero(),
        Vector size = Vector(40, 40));

    // 管理所有按钮鼠标移动事件
    void ProcessMouseEvent(InputHandler* input);

    // 清空所有按钮鼠标记录
    void ResetAllFrameStates();

    // 添加现有按钮
    void AddButton(std::shared_ptr<Button> button);

    // 移除按钮
    void RemoveButton(std::shared_ptr<Button> button);

    // 删除全部按钮
    void ClearAllButtons();

    // 统一更新所有按钮
    void UpdateAll(InputHandler* input);

    // 统一渲染所有按钮
    void DrawAll(SDL_Renderer* renderer) const;

    // 获取按钮数量
    size_t GetButtonCount() const;

    // 通过索引获取按钮
    std::shared_ptr<Button> GetButton(size_t index) const;
};

#endif
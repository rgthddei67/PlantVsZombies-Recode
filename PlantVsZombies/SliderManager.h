#pragma once
#ifndef _SLIDERMANAGER_H
#define _SLIDERMANAGER_H

#include "Slider.h"
#include <vector>
#include <memory>

class SliderManager
{
private:
    std::vector<std::shared_ptr<Slider>> sliders; // 智能指针管理滑动条

public:
    // 创建滑动条并返回指针
    std::shared_ptr<Slider> CreateSlider(Vector pos = Vector::zero(),
        Vector size = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    // 处理所有滑动条鼠标事件
    void ProcessMouseEvent(SDL_Event* event, InputHandler* input);

    // 更新所有滑动条
    void UpdateAll(InputHandler* input);

    // 渲染所有滑动条
    void DrawAll(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures) const;

    // 添加现有滑动条
    void AddSlider(std::shared_ptr<Slider> slider);

    // 移除滑动条
    void RemoveSlider(std::shared_ptr<Slider> slider);

    // 删除全部滑动条
    void ClearAllSliders();

    // 获取滑动条数量
    size_t GetSliderCount() const;

    // 通过索引获取滑动条
    std::shared_ptr<Slider> GetSlider(size_t index) const;
};

#endif
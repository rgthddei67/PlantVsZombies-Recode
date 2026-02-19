#pragma once
#ifndef _UIMANAGER_H
#define _UIMANAGER_H
#include "../ResourceManager.h"
#include "ButtonManager.h"
#include "SliderManager.h"

class UIManager
{
private:
    ButtonManager buttonManager;
    SliderManager sliderManager;

public:
    ~UIManager() 
    {
        ClearAll();
    }

    // ButtonManager 代理方法
    std::shared_ptr<Button> CreateButton(Vector pos = Vector::zero(), Vector size = Vector(40, 40))
    {
        return buttonManager.CreateButton(pos, size);
    }

    void RemoveButton(std::shared_ptr<Button> button)
    {
        buttonManager.RemoveButton(button);
    }

    void ClearAllButtons()
    {
        buttonManager.ClearAllButtons();
    }

    size_t GetButtonCount() const
    {
        return buttonManager.GetButtonCount();
    }

    std::shared_ptr<Button> GetButton(size_t index) const
    {
        return buttonManager.GetButton(index);
    }

    // SliderManager 代理方法
    std::shared_ptr<Slider> CreateSlider(Vector pos = Vector::zero(),
        Vector size = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f)
    {
        return sliderManager.CreateSlider(pos, size, minVal, maxVal, initialValue);
    }

    void RemoveSlider(std::shared_ptr<Slider> slider)
    {
        sliderManager.RemoveSlider(slider);
    }

    void ClearAllSliders()
    {
        sliderManager.ClearAllSliders();
    }

    size_t GetSliderCount() const
    {
        return sliderManager.GetSliderCount();
    }

    std::shared_ptr<Slider> GetSlider(size_t index) const
    {
        return sliderManager.GetSlider(index);
    }

    // 统一处理方法
    void ProcessMouseEvent(InputHandler* input)
    {
        buttonManager.ProcessMouseEvent(input);
        sliderManager.ProcessMouseEvent(input);
    }

    void UpdateAll(InputHandler* input)
    {
        buttonManager.UpdateAll(input);
        sliderManager.UpdateAll(input);
    }

    void DrawAll(SDL_Renderer* renderer) const
    {
        ResourceManager& resourceManager = ResourceManager::GetInstance();
        buttonManager.DrawAll(renderer);
        sliderManager.DrawAll(renderer);
    }

    void ResetAllFrameStates()
    {
        buttonManager.ResetAllFrameStates();
    }

    void ClearAll()
    {
        buttonManager.ClearAllButtons();
        sliderManager.ClearAllSliders();
    }

    // 获取底层管理器（也可以直接访问）
    ButtonManager& GetButtonManager() { return buttonManager; }
    SliderManager& GetSliderManager() { return sliderManager; }
    const ButtonManager& GetButtonManager() const { return buttonManager; }
    const SliderManager& GetSliderManager() const { return sliderManager; }
};

#endif
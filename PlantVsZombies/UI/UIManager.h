#pragma once
#ifndef _UIMANAGER_H
#define _UIMANAGER_H
#include "ResourceManager.h"
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

    void AddButton(std::shared_ptr<Button> button)
    {
        buttonManager.AddButton(button);
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

    void AddSlider(std::shared_ptr<Slider> slider)
    {
        sliderManager.AddSlider(slider);
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
    void ProcessMouseEvent(SDL_Event* event, InputHandler* input)
    {
        buttonManager.ProcessMouseEvent(event);
        sliderManager.ProcessMouseEvent(event, input);
    }

    void UpdateAll(InputHandler* input)
    {
        buttonManager.UpdateAll(input);
        sliderManager.UpdateAll(input);
    }

    void DrawAll(SDL_Renderer* renderer) const
    {
        ResourceManager& resourceManager = ResourceManager::GetInstance();

        // 获取游戏图片纹理
        const auto& imagePaths = resourceManager.GetGameImagePaths();
        std::vector<SDL_Texture*> textures;

        for (const auto& path : imagePaths)
        {
            SDL_Texture* texture = resourceManager.GetTexture(path);
            if (texture)
            {
                textures.push_back(texture);
            }
        }

        // 绘制按钮和滑动条
        buttonManager.DrawAll(renderer, textures);
        sliderManager.DrawAll(renderer, textures);
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
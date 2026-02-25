#pragma once
#ifndef _UIMANAGER_H
#define _UIMANAGER_H
#include "../ResourceManager.h"
#include "ButtonManager.h"
#include "SliderManager.h"
#include "GameMessageBox.h"         
#include "../Graphics.h"
#include "../Game/GameObjectManager.h" 

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

    // ButtonManager 浠ｇ悊鏂规硶
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

    // SliderManager 浠ｇ悊鏂规硶
    std::shared_ptr<Slider> CreateSlider(Vector pos = Vector::zero(),
        Vector size = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f)
    {
        return sliderManager.CreateSlider(pos, size, minVal, maxVal, initialValue);
    }

    // 鍒涘缓娑堟伅妗?
    std::shared_ptr<GameMessageBox> CreateMessageBox(const Vector& pos,
        const std::string& message,
        const std::vector<GameMessageBox::ButtonConfig>& buttons,
        const std::string& title = "",
        float scale = 1.0f,
        const std::string& backgroundImageKey = ResourceKeys::Textures::IMAGE_MESSAGEBOX)
    {
        return GameObjectManager::GetInstance().CreateGameObjectImmediate<GameMessageBox>(
            LAYER_UI, pos, message, buttons, title, backgroundImageKey, scale);
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

    // 缁熶竴澶勭悊鏂规硶
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

    void DrawAll(Graphics* g) const
    {
        buttonManager.DrawAll(g);
        sliderManager.DrawAll(g);
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

    // 鑾峰彇搴曞眰绠＄悊鍣紙涔熷彲浠ョ洿鎺ヨ闂級
    ButtonManager& GetButtonManager() { return buttonManager; }
    SliderManager& GetSliderManager() { return sliderManager; }
    const ButtonManager& GetButtonManager() const { return buttonManager; }
    const SliderManager& GetSliderManager() const { return sliderManager; }
};

#endif
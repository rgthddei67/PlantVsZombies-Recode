#pragma once
#ifndef _SLIDER_H
#define _SLIDER_H
#include "../Game/Definit.h"
#include "../UI/InputHandler.h"
#include "../ResourceKeys.h"
#include <functional>
#include <string>
#include <SDL2/SDL.h>
#include "../Graphics.h"

class ResourceManager;

class Slider
{
private:
    Vector position = Vector::zero();         
    Vector size = Vector(135, 10);       
    float minValue = 0.0f;                
    float maxValue = 1.0f;                
    float currentValue = 0.5f;                

    std::string backgroundImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_SLIDERSLOT;            // 鑳屾櫙鍥剧墖key
    std::string knobImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_SLIDERKNOB2;                  // 婊戝潡鍥剧墖key

    bool isDragging = false;                  
	bool canDrag = true;                      
    Vector dragStartPosition;               
    float dragStartValue;                  

    int SliderSizeX = 22;
    int SliderSizeY = 29;

    std::function<void(float)> onChangeCallback = nullptr; 

public:
    Slider(Vector createPosition = Vector::zero(),
        Vector sliderSize = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetValueRange(float min, float max);
    void SetValue(float value);

    // 是否能拖动
	void SetDrag(bool canDrag);


    void SetImageKeys(const std::string& background, const std::string& knob);


    void SetChangeCallBack(std::function<void(float)> callback);

    void ProcessMouseEvent(InputHandler* input);
    void Update(InputHandler* input);

    void Draw(Graphics* g) const;

    bool IsDragging() const;
    float GetValue() const;
    // 获取0-1的值
    float GetNormalizedValue() const; 

    bool KnobContainsPoint(Vector point) const;

    bool BackgroundContainsPoint(Vector point) const;

    float CalculateValueFromX(float x) const;

    float CalculateKnobXFromValue() const;
};
#endif
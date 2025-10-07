#pragma once
#ifndef _SLIDER_H
#define _SLIDER_H
#include "../Game/Definit.h"
#include "../UI/InputHandler.h"
#include <functional>
#include <string>
#include <SDL2/SDL.h>

class Slider
{
private:
    Vector position = Vector::zero();          // 滑动条位置（底部背景的位置）
    Vector size = Vector(135, 10);             // 滑动条大小
    float minValue = 0.0f;                     // 最小值
    float maxValue = 1.0f;                     // 最大值
    float currentValue = 0.5f;                 // 当前值

    int backgroundImageIndex = 6;             // 背景图片索引
    int knobImageIndex = 5;                   // 滑块图片索引

    bool isDragging = false;                   // 是否正在拖动
    Vector dragStartPosition;                  // 拖动开始位置
    float dragStartValue;                      // 拖动开始时的值

    int SliderSizeX = 22;
    int SliderSizeY = 29;

    std::function<void(float)> onChangeCallback = nullptr; // 值改变时候的回调

public:
    Slider(Vector createPosition = Vector::zero(),
        Vector sliderSize = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    // 设置属性
    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetValueRange(float min, float max);
    void SetValue(float value);

    // 设置图片资源索引
    void SetImageIndexes(int background, int knob);

    // 设置值改变回调
    void SetChangeCallBack(std::function<void(float)> callback);

    // 处理输入事件
    void ProcessMouseEvent(SDL_Event* event, InputHandler* input);
    void Update(InputHandler* input);

    // 渲染
    void Draw(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures) const;

    // 获取状态和值
    bool IsDragging() const;
    float GetValue() const;
    float GetNormalizedValue() const; // 获取归一化值 (0-1)

    // 检测点是否在滑块内
    bool KnobContainsPoint(Vector point) const;

    // 检测点是否在滑动条背景内
    bool BackgroundContainsPoint(Vector point) const;

    // 根据X位置计算值
    float CalculateValueFromX(float x) const;

    // 根据值计算滑块X位置
    float CalculateKnobXFromValue() const;
};
#endif
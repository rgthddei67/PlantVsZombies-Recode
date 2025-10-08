#pragma once
#ifndef _SLIDER_H
#define _SLIDER_H
#include "../Game/Definit.h"
#include "../UI/InputHandler.h"
#include <functional>
#include <string>
#include <SDL2/SDL.h>

class ResourceManager;

class Slider
{
private:
    Vector position = Vector::zero();          // ������λ�ã��ײ�������λ�ã�
    Vector size = Vector(135, 10);             // ��������С
    float minValue = 0.0f;                     // ��Сֵ
    float maxValue = 1.0f;                     // ���ֵ
    float currentValue = 0.5f;                 // ��ǰֵ

    std::string backgroundImageKey = "IMAGE_options_sliderslot";            // ����ͼƬkey
    std::string knobImageKey = "IMAGE_options_sliderknob2";                  // ����ͼƬkey

    bool isDragging = false;                   // �Ƿ������϶�
    Vector dragStartPosition;                  // �϶���ʼλ��
    float dragStartValue;                      // �϶���ʼʱ��ֵ

    int SliderSizeX = 22;
    int SliderSizeY = 29;

    std::function<void(float)> onChangeCallback = nullptr; // ֵ�ı�ʱ��Ļص�

public:
    Slider(Vector createPosition = Vector::zero(),
        Vector sliderSize = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    // ��������
    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetValueRange(float min, float max);
    void SetValue(float value);

    // ����ͼƬ��Դkey
    void SetImageKeys(const std::string& background, const std::string& knob);

    // ����ֵ�ı�ص�
    void SetChangeCallBack(std::function<void(float)> callback);

    // ���������¼�
    void ProcessMouseEvent(SDL_Event* event, InputHandler* input);
    void Update(InputHandler* input);

    // ��Ⱦ
    void Draw(SDL_Renderer* renderer) const;

    // ��ȡ״̬��ֵ
    bool IsDragging() const;
    float GetValue() const;
    float GetNormalizedValue() const; // ��ȡ��һ��ֵ (0-1)

    // �����Ƿ��ڻ�����
    bool KnobContainsPoint(Vector point) const;

    // �����Ƿ��ڻ�����������
    bool BackgroundContainsPoint(Vector point) const;

    // ����Xλ�ü���ֵ
    float CalculateValueFromX(float x) const;

    // ����ֵ���㻬��Xλ��
    float CalculateKnobXFromValue() const;
};
#endif
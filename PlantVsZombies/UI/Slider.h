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
    Vector position = Vector::zero();          // ������λ�ã��ײ�������λ�ã�
    Vector size = Vector(135, 10);             // ��������С
    float minValue = 0.0f;                     // ��Сֵ
    float maxValue = 1.0f;                     // ���ֵ
    float currentValue = 0.5f;                 // ��ǰֵ

    int backgroundImageIndex = 6;             // ����ͼƬ����
    int knobImageIndex = 5;                   // ����ͼƬ����

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

    // ����ͼƬ��Դ����
    void SetImageIndexes(int background, int knob);

    // ����ֵ�ı�ص�
    void SetChangeCallBack(std::function<void(float)> callback);

    // ���������¼�
    void ProcessMouseEvent(SDL_Event* event, InputHandler* input);
    void Update(InputHandler* input);

    // ��Ⱦ
    void Draw(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures) const;

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
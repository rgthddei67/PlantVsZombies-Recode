#pragma once
#ifndef _SLIDERMANAGER_H
#define _SLIDERMANAGER_H

#include "Slider.h"
#include <vector>
#include <memory>

class SliderManager
{
private:
    std::vector<std::shared_ptr<Slider>> sliders; // ����ָ���������

public:
    // ����������������ָ��
    std::shared_ptr<Slider> CreateSlider(Vector pos = Vector::zero(),
        Vector size = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    // �������л���������¼�
    void ProcessMouseEvent(SDL_Event* event, InputHandler* input);

    // �������л�����
    void UpdateAll(InputHandler* input);

    // ��Ⱦ���л�����
    void DrawAll(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures) const;

    // ������л�����
    void AddSlider(std::shared_ptr<Slider> slider);

    // �Ƴ�������
    void RemoveSlider(std::shared_ptr<Slider> slider);

    // ɾ��ȫ��������
    void ClearAllSliders();

    // ��ȡ����������
    size_t GetSliderCount() const;

    // ͨ��������ȡ������
    std::shared_ptr<Slider> GetSlider(size_t index) const;
};

#endif
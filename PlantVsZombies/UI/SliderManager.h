#pragma once
#ifndef _SLIDERMANAGER_H
#define _SLIDERMANAGER_H
#include "../UI/Slider.h"
#include <vector>
#include <memory>

class SliderManager
{
private:
    std::vector<std::shared_ptr<Slider>> sliders; 

public:

    std::shared_ptr<Slider> CreateSlider(Vector pos = Vector::zero(),
        Vector size = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    void ProcessMouseEvent(InputHandler* input);

    void UpdateAll(InputHandler* input);

    void DrawAll(Graphics* g) const;

    void AddSlider(std::shared_ptr<Slider> slider);

    void RemoveSlider(std::shared_ptr<Slider> slider);

    void ClearAllSliders();

    size_t GetSliderCount() const;

    std::shared_ptr<Slider> GetSlider(size_t index) const;
};

#endif
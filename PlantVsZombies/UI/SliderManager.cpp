#include "SliderManager.h"

std::shared_ptr<Slider> SliderManager::CreateSlider(Vector pos, Vector size,
    float minVal, float maxVal,
    float initialValue)
{
    auto slider = std::make_shared<Slider>(pos, size, minVal, maxVal, initialValue);
    sliders.push_back(slider);
    return slider;
}

void SliderManager::ProcessMouseEvent(InputHandler* input)
{
    for (auto& slider : sliders)
    {
        slider->ProcessMouseEvent(input);
    }
}

void SliderManager::UpdateAll(InputHandler* input)
{
    for (size_t i = 0; i < sliders.size(); i++)
    {
        sliders[i]->Update(input);
    }
}

void SliderManager::DrawAll(Graphics* g) const
{
    if (!g) return;
    for (size_t i = 0; i < sliders.size(); i++)
    {
        sliders[i]->Draw(g);
    }
}

void SliderManager::AddSlider(std::shared_ptr<Slider> slider)
{
    sliders.push_back(slider);
}

void SliderManager::RemoveSlider(std::shared_ptr<Slider> slider)
{
    for (auto it = sliders.begin(); it != sliders.end(); ++it)
    {
        if (*it == slider)
        {
            sliders.erase(it);
            break;
        }
    }
}

void SliderManager::ClearAllSliders()
{
    sliders.clear();
}

size_t SliderManager::GetSliderCount() const
{
    return sliders.size();
}

std::shared_ptr<Slider> SliderManager::GetSlider(size_t index) const
{
    if (index < sliders.size())
    {
        return sliders[index];
    }
    return nullptr;
}
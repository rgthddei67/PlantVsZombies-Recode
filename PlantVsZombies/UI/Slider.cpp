#include "Slider.h"
#include "../ResourceManager.h"
#include <iostream>
#include <algorithm>

Slider::Slider(Vector createPosition, Vector sliderSize,
    float minVal, float maxVal, float initialValue)
    : position(createPosition), size(sliderSize),
    minValue(minVal), maxValue(maxVal), currentValue(initialValue),
    isDragging(false), dragStartPosition(Vector::zero()), dragStartValue(0.0f), 
    SliderSizeX(22), SliderSizeY(29)
{
    // ȷ��ֵ����Ч��Χ��
    currentValue = std::clamp(currentValue, minValue, maxValue);
}

void Slider::SetPosition(Vector pos)
{
    this->position = pos;
}

void Slider::SetSize(Vector size)
{
    this->size = size;
}

void Slider::SetValueRange(float min, float max)
{
    this->minValue = min;
    this->maxValue = max;
    this->currentValue = std::clamp(this->currentValue, minValue, maxValue);
}

void Slider::SetValue(float value)
{
    float oldValue = this->currentValue;
    this->currentValue = std::clamp(value, minValue, maxValue);

    // ���ֵ�����仯�������˻ص������ûص�
    if (oldValue != this->currentValue && this->onChangeCallback)
    {
        this->onChangeCallback(this->currentValue);
    }
}

void Slider::SetImageKeys(const std::string& background, const std::string& knob)
{
    this->backgroundImageKey = background;
    this->knobImageKey = knob;
}

void Slider::SetChangeCallBack(std::function<void(float)> callback)
{
    this->onChangeCallback = callback;
}

void Slider::ProcessMouseEvent(SDL_Event* event, InputHandler* input)
{
    if (!input) return;

    Vector mousePos = input->GetMousePosition();

    switch (event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_LEFT)
        {
            // ����Ƿ����˻���
            if (KnobContainsPoint(mousePos))
            {
                isDragging = true;
                dragStartPosition = mousePos;
                dragStartValue = currentValue;
            }
            // ����Ƿ����˱����������ǻ��飩
            else if (BackgroundContainsPoint(mousePos) && !KnobContainsPoint(mousePos))
            {
                float newValue = CalculateValueFromX(mousePos.x);
                SetValue(newValue);
            }
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_LEFT && isDragging)
        {
            isDragging = false;
        }
        break;
    }
}

void Slider::Update(InputHandler* input)
{
    if (!input) return;

    if (isDragging)
    {
        Vector mousePos = input->GetMousePosition();
        float deltaX = mousePos.x - dragStartPosition.x;

        // �������ص�ֵ��ת������
        float pixelsPerValue = size.x / (maxValue - minValue);
        float valueDelta = deltaX / pixelsPerValue;

        SetValue(dragStartValue + valueDelta);
    }
}

void Slider::Draw(SDL_Renderer* renderer) const
{
	ResourceManager& resourceManager = ResourceManager::GetInstance();
    // ���Ʊ���
    if (!backgroundImageKey.empty() && resourceManager.HasTexture(backgroundImageKey))
    {
        SDL_Texture* texture = resourceManager.GetTexture(backgroundImageKey);
        if (texture != nullptr)
        {
            SDL_Rect destRect =
            {
                static_cast<int>(position.x),
                static_cast<int>(position.y),
                static_cast<int>(size.x),
                static_cast<int>(size.y)
            };
            SDL_RenderCopy(renderer, texture, nullptr, &destRect);
        }
    }
    else
    {
        // ���û�б���ͼƬ������һ����ɫ������Ϊ����
        SDL_Rect bgRect =
        {
            static_cast<int>(position.x),
            static_cast<int>(position.y),
            static_cast<int>(size.x),
            static_cast<int>(size.y)
        };
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // ��ɫ����
        SDL_RenderFillRect(renderer, &bgRect);

        // ���Ʊ߿�
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); // ���ɫ�߿�
        SDL_RenderDrawRect(renderer, &bgRect);
    }

    // ���㻬��λ��
    float knobX = CalculateKnobXFromValue();
    float knobY = position.y + size.y / 2; // �����ڴ�ֱ�������

    // �����С
    Vector knobSize(static_cast<float>(SliderSizeX), static_cast<float>(SliderSizeY));
    Vector knobPosition(knobX - knobSize.x / 2, knobY - knobSize.y / 2);

    // ���ƻ���
    if (!knobImageKey.empty() && resourceManager.HasTexture(knobImageKey))
    {
        SDL_Texture* texture = resourceManager.GetTexture(knobImageKey);
        if (texture != nullptr)
        {
            SDL_Rect knobRect =
            {
                static_cast<int>(knobPosition.x),
                static_cast<int>(knobPosition.y),
                static_cast<int>(knobSize.x),
                static_cast<int>(knobSize.y)
            };
            SDL_RenderCopy(renderer, texture, nullptr, &knobRect);
        }
    }
    else
    {
        // ���û�л���ͼƬ������һ��������Ϊ����
        SDL_Rect knobRect = {
            static_cast<int>(knobPosition.x),
            static_cast<int>(knobPosition.y),
            static_cast<int>(knobSize.x),
            static_cast<int>(knobSize.y)
        };

        // �����Ƿ��϶��ı���ɫ
        if (isDragging)
        {
            SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255); // �϶�ʱ��ɫ
        }
        else {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // ����ʱ��ɫ
        }
        SDL_RenderFillRect(renderer, &knobRect);

        // ���Ʊ߿�
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &knobRect);
    }
}

bool Slider::IsDragging() const
{
    return isDragging;
}

float Slider::GetValue() const
{
    return currentValue;
}

float Slider::GetNormalizedValue() const
{
    return (currentValue - minValue) / (maxValue - minValue);
}

bool Slider::KnobContainsPoint(Vector point) const
{
    float knobX = CalculateKnobXFromValue();
    float knobY = position.y + size.y / 2;
    Vector knobSize(static_cast<float>(SliderSizeX), static_cast<float>(SliderSizeY));
    Vector knobPosition(knobX - knobSize.x / 2, knobY - knobSize.y / 2);

    return point.x >= knobPosition.x && point.x <= knobPosition.x + knobSize.x &&
        point.y >= knobPosition.y && point.y <= knobPosition.y + knobSize.y;
}

bool Slider::BackgroundContainsPoint(Vector point) const
{
    return point.x >= position.x && point.x <= position.x + size.x &&
        point.y >= position.y && point.y <= position.y + size.y;
}

float Slider::CalculateValueFromX(float x) const
{
    float normalizedX = (x - position.x) / size.x;
    normalizedX = std::clamp(normalizedX, 0.0f, 1.0f);
    return minValue + normalizedX * (maxValue - minValue);
}

float Slider::CalculateKnobXFromValue() const
{
    float normalizedValue = (currentValue - minValue) / (maxValue - minValue);
    return position.x + normalizedValue * size.x;
}
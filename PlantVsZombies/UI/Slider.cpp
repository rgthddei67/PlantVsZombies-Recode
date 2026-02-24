#include "Slider.h"
#include "../ResourceManager.h"
#include "../CursorManager.h"
#include <iostream>
#include <algorithm>

Slider::Slider(Vector createPosition, Vector sliderSize,
    float minVal, float maxVal, float initialValue)
    : position(createPosition), size(sliderSize),
    minValue(minVal), maxValue(maxVal), currentValue(initialValue),
    isDragging(false), dragStartPosition(Vector::zero()), dragStartValue(0.0f), 
    SliderSizeX(22), SliderSizeY(29)
{
    // 确保值在有效范围内
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

    // 如果值发生变化且设置了回调，调用回调
    if (oldValue != this->currentValue && this->onChangeCallback)
    {
        this->onChangeCallback(this->currentValue);
    }
}

void Slider::SetDrag(bool canDrag)
{
    this->canDrag = canDrag;
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

void Slider::ProcessMouseEvent(InputHandler* input)
{
    if (!input || !canDrag) return;

    Vector mousePos = input->GetMousePosition();

    if (input->IsMouseButtonDown(SDL_BUTTON_LEFT))
    {
        // 检查是否点击了滑块
        if (KnobContainsPoint(mousePos))
        {
            isDragging = true;
            dragStartPosition = mousePos;
            dragStartValue = currentValue;
        }
        // 检查是否点击了背景（但不是滑块）
        else if (this->BackgroundContainsPoint(mousePos) && !this->KnobContainsPoint(mousePos))
        {
            float newValue = this->CalculateValueFromX(mousePos.x);
            this->SetValue(newValue);
        }
	}
    else if (input->IsMouseButtonReleased(SDL_BUTTON_LEFT) && isDragging)
    {
        isDragging = false;
	}
}

void Slider::Update(InputHandler* input)
{
    if (!input) return;

    Vector mousePos = input->GetMouseWorldPosition();

    if (BackgroundContainsPoint(mousePos) && canDrag)
    {
		CursorManager::GetInstance().IncrementHoverCount();
    }

    if (isDragging)
    {
        float deltaX = mousePos.x - dragStartPosition.x;

        // 计算像素到值的转换比例
        float pixelsPerValue = size.x / (maxValue - minValue);
        float valueDelta = deltaX / pixelsPerValue;

        SetValue(dragStartValue + valueDelta);
    }
}

void Slider::Draw(Graphics* g) const
{
	ResourceManager& resourceManager = ResourceManager::GetInstance();
    // 绘制背景
    if (!backgroundImageKey.empty() && resourceManager.HasTexture(backgroundImageKey))
    {
        const GLTexture* texture = resourceManager.GetTexture(backgroundImageKey);
        if (texture != nullptr)
        {
            g->DrawTexture(texture, position.x, position.y, size.x, size.y);
        }
    }
    else
    {
        // 如果没有背景图片，绘制一个灰色矩形作为背景
        g->FillRect(position.x, position.y, size.x, size.y);
        g->DrawRect(position.x, position.y, size.x, size.y);
    }

    // 计算滑块位置
    float knobX = CalculateKnobXFromValue();
    float knobY = position.y + size.y / 2; // 滑块在垂直方向居中

    // 滑块大小
    Vector knobSize(static_cast<float>(SliderSizeX), static_cast<float>(SliderSizeY));
    Vector knobPosition(knobX - knobSize.x / 2, knobY - knobSize.y / 2);

    // 绘制滑块
    if (!knobImageKey.empty() && resourceManager.HasTexture(knobImageKey))
    {
        const GLTexture* texture2 = resourceManager.GetTexture(knobImageKey);
        if (texture2 != nullptr)
        {
            g->DrawTexture(texture2, knobPosition.x, knobPosition.y, knobSize.x, knobSize.y);
        }
    }
    else
    {
        std::cerr << 
            "没有滑块！直接不绘制了，我草你妈，怎么图片都不搞？知不知道写这玩意麻烦死了!" << std::endl;
    }
}

bool Slider::IsDragging() const
{
    return isDragging;
}

float Slider::GetValue() const
{
    return this->currentValue;
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
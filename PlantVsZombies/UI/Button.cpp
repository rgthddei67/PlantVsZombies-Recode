#include "Button.h"
#include "InputHandler.h"
#include <iostream>
#include <SDL2/SDL_image.h>
#include "../Game/AudioSystem.h"
#include "../CursorManager.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"

std::string Button::s_defaultFontPath = ResourceKeys::Fonts::FONT_FZCQ;

void Button::SetDefaultFontPath(const std::string& path)
{
    s_defaultFontPath = path;
}

std::string Button::GetDefaultFontPath()
{
    return s_defaultFontPath;
}

Button::Button(Vector createPosition, Vector btnSize)
    : position(createPosition), size(btnSize), isHovered(false), isPressed(false),
    isChecked(false), isCheckbox(false), fontName(s_defaultFontPath), fontSize(17), 
    m_mousePressedThisFrame(false),
    m_mouseReleasedThisFrame(false), m_wasMouseDown(false)
{
}

void Button::SetPosition(Vector pos)
{
    this->position = pos;
}

void Button::SetSize(Vector size)
{
    this->size = size;
}

void Button::SetText(const std::string& btnText, const std::string& font, int size)
{
    this->text = btnText;
    this->fontName = font;
    this->fontSize = size;
}

void Button::SetTextColor(const SDL_Color& color)
{
    this->textColor = color;
}

void Button::SetHoverTextColor(const SDL_Color& color)
{
    this->hoverTextColor = color;
}

void Button::SetAsCheckbox(bool checkbox)
{
    this->isCheckbox = checkbox;
}

void Button::SetCanClick(bool canClick)
{
    this->canClick = canClick;
}

void Button::SetImageKeys(const std::string& normal, const std::string& hover, const std::string& pressed, const std::string& checked)
{
    this->normalImageKey = normal;
    this->hoverImageKey = hover;
    this->pressedImageKey = pressed;
    this->checkedImageKey = checked;
}

void Button::SetClickCallBack(std::function<void(bool)> callback)
{
    this->onClickCallback = callback;
}

void Button::ProcessMouseEvent(InputHandler* input)
{
	if (input == nullptr || !canClick || !mEnabled) return;

    if (input->IsMouseButtonPressed(SDL_BUTTON_LEFT))
    {
        m_mousePressedThisFrame = true;
    }

	if (input->IsMouseButtonReleased(SDL_BUTTON_LEFT))
    {
        m_mouseReleasedThisFrame = true;
	}
}

void Button::ResetFrameState()
{
    m_mousePressedThisFrame = false;
    m_mouseReleasedThisFrame = false;
}

void Button::Update(InputHandler* input)
{
    if (!input || !mEnabled) return;

    Vector mousePos = input->GetMousePosition();
    this->isHovered = this->ContainsPoint(mousePos);

    // 处理悬停状态变化
    if (this->isHovered) {
        CursorManager::GetInstance().IncrementHoverCount();
    }

    // 处理鼠标按下
    if (this->isHovered && m_mousePressedThisFrame)
    {
        this->isPressed = true;
        // 按下时立即触发复选框状态切换
        if (isCheckbox && !m_wasMouseDown)
        {
            // 复选框可以在按下时就切换状态
            this->isChecked = !this->isChecked;
        }
    }

    // 处理鼠标释放
    if (m_mouseReleasedThisFrame)
    {
        if (this->isPressed && this->isHovered && this->onClickCallback)
        {
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BUTTONCLICK, 0.5f);
            this->onClickCallback(this->isChecked);
        }
        this->isPressed = false;
    }

    // 更新鼠标状态记录
    m_wasMouseDown = input->IsMouseButtonDown(SDL_BUTTON_LEFT);
    ResetFrameState();
}

void Button::Draw(SDL_Renderer* renderer) const
{
    if (!mEnabled) return;

	ResourceManager& resourceManager = ResourceManager::GetInstance();
    // 确定要显示的图片key
    std::string imageKey = normalImageKey;

    if (this->isCheckbox && this->isChecked && !this->checkedImageKey.empty())
    {
        imageKey = this->checkedImageKey;
    }
    else if (this->isPressed && !this->pressedImageKey.empty())
    {
        imageKey = this->pressedImageKey;
    }
    else if (this->isHovered && !this->hoverImageKey.empty())
    {
        imageKey = this->hoverImageKey;
    }

    // 绘制按钮图片
    if (!imageKey.empty() && resourceManager.HasTexture(imageKey))
    {
        SDL_Texture* texture = resourceManager.GetTexture(imageKey);
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
        // 如果没有图片，绘制一个矩形作为按钮
        SDL_Rect rect = {
            static_cast<int>(position.x),
            static_cast<int>(position.y),
            static_cast<int>(size.x),
            static_cast<int>(size.y)
        };

        // 设置颜色（悬停时为浅灰色，否则为白色）
        if (isHovered) {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // 浅灰色
        }
        else {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // 白色
        }
        SDL_RenderFillRect(renderer, &rect);

        // 绘制边框
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // 黑色边框
        SDL_RenderDrawRect(renderer, &rect);
    }

    // 绘制文本
    if (!text.empty())
    {
        SDL_Color color = this->isHovered ? this->hoverTextColor : this->textColor;

        // 使用TTF测量文本实际宽度（修复UTF-8中文问题）
        int textWidth = 0;
        int textHeight = fontSize;

        // 临时加载字体来测量文本尺寸
        TTF_Font* tempFont = resourceManager.GetFont(fontName, fontSize);
        if (tempFont)
        {
            TTF_SizeUTF8(tempFont, text.c_str(), &textWidth, &textHeight);
        }
        else
        {
            // 如果字体加载失败，使用估算值（每个中文字符约等于fontSize宽度）
            textWidth = 0;
            for (size_t i = 0; i < text.length(); )
            {
                unsigned char c = text[i];
                if ((c & 0x80) == 0)
                {
                    // ASCII字符，宽度约为fontSize/2
                    textWidth += fontSize / 2;
                    i += 1;
                }
                else if ((c & 0xE0) == 0xC0)
                {
                    // 2字节UTF-8字符（如中文）
                    textWidth += fontSize;
                    i += 2;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    // 3字节UTF-8字符
                    textWidth += fontSize;
                    i += 3;
                }
                else
                {
                    // 其他情况
                    textWidth += fontSize;
                    i += 1;
                }
            }
        }

        int textX = static_cast<int>(position.x + (size.x - textWidth) / 2);
        int textY = static_cast<int>(position.y + (size.y - textHeight) / 2);

        GameAPP::GetInstance().DrawText(text, textX, textY, color);
    }
}

bool Button::IsHovered() const
{
    return this->isHovered;
}

bool Button::IsPressed() const
{
    return this->isPressed;
}

bool Button::IsChecked() const
{
    return this->isChecked;
}

void Button::SetChecked(bool checked)
{
    this->isChecked = checked;
}

bool Button::ContainsPoint(Vector point) const
{
    return point.x >= position.x && point.x <= position.x + size.x &&
        point.y >= position.y && point.y <= position.y + size.y;
}
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

void Button::SetTextColor(const glm::vec4& color)
{
    this->textColor = color;
}

void Button::SetHoverTextColor(const glm::vec4& color)
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

    // 鼠标变化
    if (this->isHovered) {
        CursorManager::GetInstance().IncrementHoverCount();
    }

    if (this->isHovered && m_mousePressedThisFrame)
    {
        this->isPressed = true;
        if (isCheckbox && !m_wasMouseDown)
        {
            this->isChecked = !this->isChecked;
        }
    }

    if (m_mouseReleasedThisFrame)
    {
        if (this->isPressed && this->isHovered && this->onClickCallback)
        {
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BUTTONCLICK, 0.5f);
            this->onClickCallback(this->isChecked);
        }
        this->isPressed = false;
    }

    m_wasMouseDown = input->IsMouseButtonDown(SDL_BUTTON_LEFT);
    ResetFrameState();
}

void Button::Draw(Graphics* g) const
{
    if (!mEnabled || !g) return;

	ResourceManager& resourceManager = ResourceManager::GetInstance();

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

    if (!imageKey.empty() && resourceManager.HasTexture(imageKey))
    {
        const GLTexture* texture = resourceManager.GetTexture(imageKey);
        if (texture != nullptr)
        {
            Vector pos = g->ScreenToWorldPosition(position.x, position.y);
            g->DrawTexture(texture, pos.x, pos.y, size.x, size.y);
        }
    }
    else
    {
        std::cerr <<
            "[Button]: 没有绘制图片!" << std::endl;
    }

    if (!text.empty())
    {
        glm::vec4 color = this->isHovered ? this->hoverTextColor : this->textColor;

        int textWidth = 0;
        int textHeight = 0;
        TTF_Font* tempFont = resourceManager.GetFont(fontName, fontSize);
        if (tempFont)
        {
            TTF_SizeUTF8(tempFont, text.c_str(), &textWidth, &textHeight);
        }
        else
        {
            textWidth = 0;
            for (size_t i = 0; i < text.length(); )
            {
                unsigned char c = text[i];
                if ((c & 0x80) == 0)
                {
                    textWidth += fontSize / 2; // ASCII
                    i += 1;
                }
                else if ((c & 0xE0) == 0xC0)
                {
                    textWidth += fontSize;     // 2 UTF8
                    i += 2;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    textWidth += fontSize;     // 3 UTF8
                    i += 3;
                }
                else
                {
                    textWidth += fontSize;     
                    i += 1;
                }
            }
            textHeight = fontSize; 
        }

        float textX = position.x + (size.x - static_cast<float>(textWidth)) / 2;
        float textY = position.y + (size.y - static_cast<float>(textHeight)) / 2;

        Vector textPos = g->ScreenToWorldPosition(textX, textY);

        GameAPP::GetInstance().DrawText(text, textPos, color, fontName, fontSize);
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
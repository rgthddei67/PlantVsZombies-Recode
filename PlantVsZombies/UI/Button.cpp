#include "Button.h"
#include "InputHandler.h"
#include <iostream>
#include <SDL_image.h>
#include "../ResourceManager.h"

std::string Button::s_defaultFontPath = "./font/fzcq.ttf";

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
	isChecked(false), isCheckbox(false), normalImageIndex(-1),
	hoverImageIndex(-1), pressedImageIndex(-1), checkedImageIndex(-1),
	fontName(s_defaultFontPath), fontSize(17), m_mousePressedThisFrame(false), 
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

void Button::SetImageIndexes(int normal, int hover, int pressed, int checked)
{
	this->normalImageIndex = normal;
	this->hoverImageIndex = hover;
	this->pressedImageIndex = pressed;
	this->checkedImageIndex = checked;
}

void Button::SetClickCallBack(std::function<void()> callback)
{
	this->onClickCallback = callback;
}

void Button::ProcessMouseEvent(SDL_Event* event)
{
	switch (event->type) {
	case SDL_MOUSEBUTTONDOWN:
		if (event->button.button == SDL_BUTTON_LEFT) {
			m_mousePressedThisFrame = true;
		}
		break;
	case SDL_MOUSEBUTTONUP:
		if (event->button.button == SDL_BUTTON_LEFT) {
			m_mouseReleasedThisFrame = true;
		}
		break;
	}
}

void Button::ResetFrameState()
{
	m_mousePressedThisFrame = false;
	m_mouseReleasedThisFrame = false;
}

void Button::Update(InputHandler* input)
{
	Vector mousePos = input->GetMousePosition();
	this->isHovered = this->ContainsPoint(mousePos);

	// ʹ��ʵ��������ⰴ��
	if (this->isHovered && m_mousePressedThisFrame)
	{
		
		this->isPressed = true;
	}

	// ����ͷ�
	if (this->isPressed && m_mouseReleasedThisFrame)
	{
		this->isPressed = false;

		if (this->isHovered)
		{
			if (this->isCheckbox)
			{
				this->isChecked = !this->isChecked;
			}
			if (this->onClickCallback)
			{
				this->onClickCallback();
			}
		}
	}
}

void Button::Draw(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures) const
{
	// ȷ��Ҫ��ʾ��ͼƬ����
	int imageIndex = normalImageIndex;

	if (this->isCheckbox && this->isChecked && this->checkedImageIndex != -1)
	{
		imageIndex = this->checkedImageIndex;
	}
	else if (this->isPressed && this->pressedImageIndex != -1)
	{
		imageIndex = this->pressedImageIndex;
	}
	else if (this->isHovered && this->hoverImageIndex != -1)
	{
		imageIndex = this->hoverImageIndex;
	}

	// ���ư�ťͼƬ
	if (imageIndex >= 0 && imageIndex < textures.size() && textures[imageIndex] != nullptr)
	{
		SDL_Rect destRect = 
		{
			static_cast<int>(position.x),
			static_cast<int>(position.y),
			static_cast<int>(size.x),
			static_cast<int>(size.y)
		};
		SDL_RenderCopy(renderer, textures[imageIndex], nullptr, &destRect);
	}
	else
	{
		// ���û��ͼƬ������һ��������Ϊ��ť
		SDL_Rect rect = {
			static_cast<int>(position.x),
			static_cast<int>(position.y),
			static_cast<int>(size.x),
			static_cast<int>(size.y)
		};

		// ������ɫ����ͣʱΪǳ��ɫ������Ϊ��ɫ��
		if (isHovered) {
			SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // ǳ��ɫ
		}
		else {
			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // ��ɫ
		}
		SDL_RenderFillRect(renderer, &rect);

		// ���Ʊ߿�
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // ��ɫ�߿�
		SDL_RenderDrawRect(renderer, &rect);
	}

	// �����ı�
	if (!text.empty())
	{
		SDL_Color color = this->isHovered ? this->hoverTextColor : this->textColor;

		// ʹ��TTF�����ı�ʵ�ʿ�ȣ��޸�UTF-8�������⣩
		int textWidth = 0;
		int textHeight = fontSize;

		// ��ʱ���������������ı��ߴ�
		TTF_Font* tempFont = ResourceManager::GetInstance().GetFont(fontName, fontSize);
		if (tempFont)
		{
			TTF_SizeUTF8(tempFont, text.c_str(), &textWidth, &textHeight);
		}
		else
		{
			// ����������ʧ�ܣ�ʹ�ù���ֵ��ÿ�������ַ�Լ����fontSize��ȣ�
			textWidth = 0;
			for (size_t i = 0; i < text.length(); )
			{
				unsigned char c = text[i];
				if ((c & 0x80) == 0)
				{
					// ASCII�ַ������ԼΪfontSize/2
					textWidth += fontSize / 2;
					i += 1;
				}
				else if ((c & 0xE0) == 0xC0)
				{
					// 2�ֽ�UTF-8�ַ��������ģ�
					textWidth += fontSize;
					i += 2;
				}
				else if ((c & 0xF0) == 0xE0)
				{
					// 3�ֽ�UTF-8�ַ�
					textWidth += fontSize;
					i += 3;
				}
				else
				{
					// �������
					textWidth += fontSize;
					i += 1;
				}
			}
		}

		int textX = static_cast<int>(position.x + (size.x - textWidth) / 2);
		int textY = static_cast<int>(position.y + (size.y - textHeight) / 2);

		GameAPP::DrawText(renderer, text, textX, textY, color);
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

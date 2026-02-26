#pragma once
#ifndef _BUTTON_H
#define _BUTTON_H
#include "../GameApp.h"
#include "../Graphics.h"
#include <functional>
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class ResourceManager;

class Button
{
private:
    Vector position = Vector::zero();         
    Vector size = Vector(40, 40);         
    bool isHovered = false;                  
    bool isPressed = false;               
    bool isChecked = false;                   
    bool isCheckbox = false;               
	bool canClick = true;                      

    std::string normalImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0;              
    std::string hoverImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0;               
    std::string pressedImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0;
    std::string checkedImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX1;

    std::string text = "";                   
    std::string fontName = ResourceKeys::Fonts::FONT_FZCQ;      
    int fontSize = 17;                    
    glm::vec4 textColor = { 0, 0, 0, 255 };      
    glm::vec4 hoverTextColor = { 255, 255, 255, 255 };

    std::function<void(bool isChecked)> onClickCallback = nullptr; 
    static std::string s_defaultFontPath;
    bool m_mousePressedThisFrame;
    bool m_mouseReleasedThisFrame;
    bool m_wasMouseDown;

	bool mEnabled = true; 

public:
    Button(Vector createPosition = Vector::zero(), Vector btnSize = Vector(40, 40));
    static void SetDefaultFontPath(const std::string& path);
    static std::string GetDefaultFontPath();

    void ProcessMouseEvent(InputHandler* input);
    void ResetFrameState(); 

    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetText(const std::string& btnText, const std::string& font = ResourceKeys::Fonts::FONT_FZCQ, int size = 17);
    void SetTextColor(const glm::vec4& color);
    void SetHoverTextColor(const glm::vec4& color);
    void SetAsCheckbox(bool checkbox);
	void SetCanClick(bool canClick);
    void SetEnabled(bool enabled) { this->mEnabled = enabled; }

    void SetImageKeys(const std::string& normal, const std::string& hover = "", const std::string& pressed = "", const std::string& checked = "");

    void SetClickCallBack(std::function<void(bool)> callback);

    void Update(InputHandler* input);
    void Draw(Graphics* g) const;

    bool IsHovered() const;
    bool IsPressed() const;
    bool IsChecked() const;
    void SetChecked(bool checked);

    bool ContainsPoint(Vector point) const;
};

#endif
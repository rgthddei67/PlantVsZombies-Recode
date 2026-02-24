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
    Vector position = Vector::zero();          // 按钮位置
    Vector size = Vector(40, 40);              // 按钮大小
    bool isHovered = false;                    // 是否悬停
    bool isPressed = false;                    // 是否按下
    bool isChecked = false;                    // 是否勾选
    bool isCheckbox = false;                   // 是否是复选框类型
	bool canClick = true;                      // 是否可点击

    std::string normalImageKey = "IMAGE_options_checkbox0";                // 正常状态图片key
    std::string hoverImageKey = "IMAGE_options_checkbox0";                 // 悬停状态图片key  
    std::string pressedImageKey = "IMAGE_options_checkbox0";               // 按下状态图片key
    std::string checkedImageKey = "IMAGE_options_checkbox1";               // 选中状态图片key

    std::string text = "";                     // 按钮文字
    std::string fontName = ResourceKeys::Fonts::FONT_FZCQ;        // 字体文件名
    int fontSize = 17;                         // 字体大小
    glm::vec4 textColor = { 0, 0, 0, 255 };      // 文字颜色（黑色）
    glm::vec4 hoverTextColor = { 255, 255, 255, 255 }; // 悬停时文字颜色（白色）

    std::function<void(bool isChecked)> onClickCallback = nullptr; // 点击回调函数
    static std::string s_defaultFontPath;
    bool m_mousePressedThisFrame;
    bool m_mouseReleasedThisFrame;
    bool m_wasMouseDown;

	bool mEnabled = true; // 是否启用按钮

public:
    Button(Vector createPosition = Vector::zero(), Vector btnSize = Vector(40, 40));
    static void SetDefaultFontPath(const std::string& path);
    static std::string GetDefaultFontPath();

    void ProcessMouseEvent(InputHandler* input);
    void ResetFrameState(); // 重置

    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetText(const std::string& btnText, const std::string& font = ResourceKeys::Fonts::FONT_FZCQ, int size = 17);
    void SetTextColor(const glm::vec4& color);
    void SetHoverTextColor(const glm::vec4& color);
    void SetAsCheckbox(bool checkbox);
	void SetCanClick(bool canClick);
    void SetEnabled(bool enabled) { this->mEnabled = enabled; }

    // 设置图片资源key
    void SetImageKeys(const std::string& normal, const std::string& hover = "", const std::string& pressed = "", const std::string& checked = "");

    // 设置点击回调
    void SetClickCallBack(std::function<void(bool)> callback);

    // 更新和渲染
    void Update(InputHandler* input);
    void Draw(Graphics* g) const;

    // 状态获取
    bool IsHovered() const;
    bool IsPressed() const;
    bool IsChecked() const;
    void SetChecked(bool checked);

    // 检测点是否在按钮内
    bool ContainsPoint(Vector point) const;
};

#endif
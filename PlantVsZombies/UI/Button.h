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
    Vector position = Vector::zero();          // 鎸夐挳浣嶇疆
    Vector size = Vector(40, 40);              // 鎸夐挳澶у皬
    bool isHovered = false;                    // 鏄惁鎮仠
    bool isPressed = false;                    // 鏄惁鎸変笅
    bool isChecked = false;                    // 鏄惁鍕鹃€?
    bool isCheckbox = false;                   // 鏄惁鏄閫夋绫诲瀷
	bool canClick = true;                      // 鏄惁鍙偣鍑?

    std::string normalImageKey = "IMAGE_options_checkbox0";                // 姝ｅ父鐘舵€佸浘鐗噆ey
    std::string hoverImageKey = "IMAGE_options_checkbox0";                 // 鎮仠鐘舵€佸浘鐗噆ey  
    std::string pressedImageKey = "IMAGE_options_checkbox0";               // 鎸変笅鐘舵€佸浘鐗噆ey
    std::string checkedImageKey = "IMAGE_options_checkbox1";               // 閫変腑鐘舵€佸浘鐗噆ey

    std::string text = "";                     // 鎸夐挳鏂囧瓧
    std::string fontName = ResourceKeys::Fonts::FONT_FZCQ;        // 瀛椾綋鏂囦欢鍚?
    int fontSize = 17;                         // 瀛椾綋澶у皬
    glm::vec4 textColor = { 0, 0, 0, 255 };      // 鏂囧瓧棰滆壊锛堥粦鑹诧級
    glm::vec4 hoverTextColor = { 255, 255, 255, 255 }; // 鎮仠鏃舵枃瀛楅鑹诧紙鐧借壊锛?

    std::function<void(bool isChecked)> onClickCallback = nullptr; // 鐐瑰嚮鍥炶皟鍑芥暟
    static std::string s_defaultFontPath;
    bool m_mousePressedThisFrame;
    bool m_mouseReleasedThisFrame;
    bool m_wasMouseDown;

	bool mEnabled = true; // 鏄惁鍚敤鎸夐挳

public:
    Button(Vector createPosition = Vector::zero(), Vector btnSize = Vector(40, 40));
    static void SetDefaultFontPath(const std::string& path);
    static std::string GetDefaultFontPath();

    void ProcessMouseEvent(InputHandler* input);
    void ResetFrameState(); // 閲嶇疆

    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetText(const std::string& btnText, const std::string& font = ResourceKeys::Fonts::FONT_FZCQ, int size = 17);
    void SetTextColor(const glm::vec4& color);
    void SetHoverTextColor(const glm::vec4& color);
    void SetAsCheckbox(bool checkbox);
	void SetCanClick(bool canClick);
    void SetEnabled(bool enabled) { this->mEnabled = enabled; }

    // 璁剧疆鍥剧墖璧勬簮key
    void SetImageKeys(const std::string& normal, const std::string& hover = "", const std::string& pressed = "", const std::string& checked = "");

    // 璁剧疆鐐瑰嚮鍥炶皟
    void SetClickCallBack(std::function<void(bool)> callback);

    // 鏇存柊鍜屾覆鏌?
    void Update(InputHandler* input);
    void Draw(Graphics* g) const;

    // 鐘舵€佽幏鍙?
    bool IsHovered() const;
    bool IsPressed() const;
    bool IsChecked() const;
    void SetChecked(bool checked);

    // 妫€娴嬬偣鏄惁鍦ㄦ寜閽唴
    bool ContainsPoint(Vector point) const;
};

#endif
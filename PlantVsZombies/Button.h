#pragma once
#ifndef _BUTTON_H
#define _BUTTON_H
#include "AllCppInclude.h"
#include "GameApp.h"
#include <functional>
#include <string>
#include <SDL.h>
#include <SDL_ttf.h>

class Button
{
private:
    Vector position = Vector::zero();          // 按钮位置
    Vector size = Vector(40, 40);              // 按钮大小
    bool isHovered = false;                    // 是否悬停
    bool isPressed = false;                    // 是否按下
    bool isChecked = false;                    // 是否勾选
    bool isCheckbox = false;                   // 是否是复选框类型

    int normalImageIndex = -1;                 // 正常状态图片索引
    int hoverImageIndex = -1;                  // 悬停状态图片索引  
    int pressedImageIndex = -1;                // 按下状态图片索引
    int checkedImageIndex = -1;                // 选中状态图片索引

    std::string text = "";                     // 按钮文字
    std::string fontName = "./font/fzcq.ttf";        // 字体文件名
    int fontSize = 17;                         // 字体大小
    SDL_Color textColor = { 0, 0, 0, 255 };      // 文字颜色（黑色）
    SDL_Color hoverTextColor = { 255, 255, 255, 255 }; // 悬停时文字颜色（白色）

    std::function<void()> onClickCallback = nullptr; // 点击回调函数
    static std::string s_defaultFontPath;
    bool m_mousePressedThisFrame;
    bool m_mouseReleasedThisFrame;
    bool m_wasMouseDown;

public:
    Button(Vector createPosition = Vector::zero(), Vector btnSize = Vector(40, 40));
    static void SetDefaultFontPath(const std::string& path);
    static std::string GetDefaultFontPath();
    void ProcessMouseEvent(SDL_Event* event);
    void ResetFrameState(); // 重置
    // 设置按钮属性
    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetText(const std::string& btnText, const std::string& font = "./font/fzcq.ttf", int size = 17);
    void SetTextColor(const SDL_Color& color);
    void SetHoverTextColor(const SDL_Color& color);
    void SetAsCheckbox(bool checkbox);

    // 设置图片资源索引
    void SetImageIndexes(int normal, int hover = -1, int pressed = -1, int checked = -1);

    // 设置点击回调
    void SetClickCallBack(std::function<void()> callback);

    // 更新和渲染
    void Update(InputHandler* input);
    void Draw(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures) const;

    // 状态获取
    bool IsHovered() const;
    bool IsPressed() const;
    bool IsChecked() const;
    void SetChecked(bool checked);

    // 检测点是否在按钮内
    bool ContainsPoint(Vector point) const;
};

#endif
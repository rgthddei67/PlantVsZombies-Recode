#pragma once
#ifndef _H_MESSAGEBOX_H
#define _H_MESSAGEBOX_H

#include "../Game/GameObject.h"
#include "Button.h"
#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>

class GameMessageBox : public GameObject {
public:
    // 按钮配置结构
    struct ButtonConfig {
        std::string text;                 // 按钮文字
        Vector pos;
        std::function<void()> callback;   // 点击回调
        bool autoClose = true;             // 点击后是否自动关闭消息框
    };

    GameMessageBox(const Vector& pos,
        const std::string& message,
        const std::vector<ButtonConfig>& buttons,
        const std::string& title = "",
        const std::string& backgroundImageKey = "",
        float scale = 1.0f);

    ~GameMessageBox();

    virtual void Start() override;
    virtual void Draw(SDL_Renderer* renderer) override;

    // 关闭消息框（从GameObjectManager中销毁自己）
    void Close();

private:
    Vector m_position;
    float m_scale;
    Vector m_size;               // 实际大小（已乘缩放）
    std::string m_title;
    std::string m_message;
    std::string m_backgroundImageKey = ResourceKeys::Textures::IMAGE_MESSAGEBOX;
    std::vector<ButtonConfig> m_buttonConfigs;
    std::vector<std::shared_ptr<Button>> m_buttons;

    SDL_Color m_textColor = { 245, 214, 127, 255 };
    SDL_Color m_titleColor = { 53, 191, 61, 255 };

    // 获取背景图片原始尺寸（若无图片则返回默认尺寸）
    Vector GetBackgroundOriginalSize() const;
};

#endif
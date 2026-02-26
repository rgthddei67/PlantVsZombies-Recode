#pragma once
#ifndef _H_MESSAGEBOX_H
#define _H_MESSAGEBOX_H

#include "../Game/GameObject.h"
#include "../Graphics.h"
#include "Button.h"
#include "Slider.h"
#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>

class GameMessageBox : public GameObject {
public:
    struct ButtonConfig {
        std::string text;               
        Vector pos;
        Vector size;   // 大小，如果是Vector::zero就是按照NormalButton处理
        float fontsize;
        std::function<void()> callback; 
        std::string texture;
        bool autoClose = true;             // 是否自动关闭
    };

    struct SliderConfig {
        Vector pos;
        Vector size;   
        float min;
        float max;
        float initValue;    // 初始化的值
        std::function<void(float)> callback;
    };

    struct TextConfig {
        Vector pos;
        float size;
        std::string text;
        glm::vec4 color;
        std::string font = ResourceKeys::Fonts::FONT_FZCQ;
    };

    GameMessageBox(const Vector& pos,
        const std::string& message,
        const std::vector<ButtonConfig>& buttons,
        const std::vector<SliderConfig>& sliders,
        const std::vector<TextConfig>& texts,
        const std::string& title = "",
        const std::string& backgroundImageKey = "",
        float scale = 1.0f);

    ~GameMessageBox();

    virtual void Start() override;
    virtual void Draw(Graphics* g) override;

    void Close();

private:
    Vector m_position;
    float m_scale;
    Vector m_size;             
    std::string m_title;
    std::string m_message;
    std::string m_backgroundImageKey = ResourceKeys::Textures::IMAGE_MESSAGEBOX;
    std::vector<ButtonConfig> m_buttonConfigs;
    std::vector<SliderConfig> m_sliderConfigs;
    std::vector<TextConfig> m_textConfigs;

    std::vector<std::shared_ptr<Button>> m_buttons;
    std::vector<std::shared_ptr<Slider>> m_sliders;

    glm::vec4 m_textColor = { 245, 214, 127, 255 };
    glm::vec4 m_titleColor = { 53, 191, 61, 255 };

    Vector GetBackgroundOriginalSize() const;
};

#endif
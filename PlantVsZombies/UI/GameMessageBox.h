#pragma once
#ifndef _H_MESSAGEBOX_H
#define _H_MESSAGEBOX_H

#include "../Game/GameObject.h"
#include "../Graphics.h"
#include "Button.h"
#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>

class GameMessageBox : public GameObject {
public:
    // 鎸夐挳閰嶇疆缁撴瀯
    struct ButtonConfig {
        std::string text;                 // 鎸夐挳鏂囧瓧
        Vector pos;
        std::function<void()> callback;   // 鐐瑰嚮鍥炶皟
        bool autoClose = true;             // 鐐瑰嚮鍚庢槸鍚﹁嚜鍔ㄥ叧闂秷鎭
    };

    GameMessageBox(const Vector& pos,
        const std::string& message,
        const std::vector<ButtonConfig>& buttons,
        const std::string& title = "",
        const std::string& backgroundImageKey = "",
        float scale = 1.0f);

    ~GameMessageBox();

    virtual void Start() override;
    virtual void Draw(Graphics* g) override;

    // 鍏抽棴娑堟伅妗嗭紙浠嶨ameObjectManager涓攢姣佽嚜宸憋級
    void Close();

private:
    Vector m_position;
    float m_scale;
    Vector m_size;               // 瀹為檯澶у皬锛堝凡涔樼缉鏀撅級
    std::string m_title;
    std::string m_message;
    std::string m_backgroundImageKey = ResourceKeys::Textures::IMAGE_MESSAGEBOX;
    std::vector<ButtonConfig> m_buttonConfigs;
    std::vector<std::shared_ptr<Button>> m_buttons;

    glm::vec4 m_textColor = { 245, 214, 127, 255 };
    glm::vec4 m_titleColor = { 53, 191, 61, 255 };

    // 鑾峰彇鑳屾櫙鍥剧墖鍘熷灏哄锛堣嫢鏃犲浘鐗囧垯杩斿洖榛樿灏哄锛?
    Vector GetBackgroundOriginalSize() const;
};

#endif
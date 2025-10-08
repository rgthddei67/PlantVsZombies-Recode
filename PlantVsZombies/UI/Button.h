#pragma once
#ifndef _BUTTON_H
#define _BUTTON_H
#include "../GameApp.h"
#include <functional>
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class ResourceManager;

class Button
{
private:
    Vector position = Vector::zero();          // ��ťλ��
    Vector size = Vector(40, 40);              // ��ť��С
    bool isHovered = false;                    // �Ƿ���ͣ
    bool isPressed = false;                    // �Ƿ���
    bool isChecked = false;                    // �Ƿ�ѡ
    bool isCheckbox = false;                   // �Ƿ��Ǹ�ѡ������

    std::string normalImageKey;                // ����״̬ͼƬkey
    std::string hoverImageKey;                 // ��ͣ״̬ͼƬkey  
    std::string pressedImageKey;               // ����״̬ͼƬkey
    std::string checkedImageKey;               // ѡ��״̬ͼƬkey

    std::string text = "";                     // ��ť����
    std::string fontName = "./font/fzcq.ttf";        // �����ļ���
    int fontSize = 17;                         // �����С
    SDL_Color textColor = { 0, 0, 0, 255 };      // ������ɫ����ɫ��
    SDL_Color hoverTextColor = { 255, 255, 255, 255 }; // ��ͣʱ������ɫ����ɫ��

    std::function<void()> onClickCallback = nullptr; // ����ص�����
    static std::string s_defaultFontPath;
    bool m_mousePressedThisFrame;
    bool m_mouseReleasedThisFrame;
    bool m_wasMouseDown;

public:
    Button(Vector createPosition = Vector::zero(), Vector btnSize = Vector(40, 40));
    static void SetDefaultFontPath(const std::string& path);
    static std::string GetDefaultFontPath();
    void ProcessMouseEvent(SDL_Event* event);
    void ResetFrameState(); // ����
    // ���ð�ť����
    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetText(const std::string& btnText, const std::string& font = "./font/fzcq.ttf", int size = 17);
    void SetTextColor(const SDL_Color& color);
    void SetHoverTextColor(const SDL_Color& color);
    void SetAsCheckbox(bool checkbox);

    // ����ͼƬ��Դkey
    void SetImageKeys(const std::string& normal, const std::string& hover = "", const std::string& pressed = "", const std::string& checked = "");

    // ���õ���ص�
    void SetClickCallBack(std::function<void()> callback);

    // ���º���Ⱦ
    void Update(InputHandler* input);
    void Draw(SDL_Renderer* renderer) const;

    // ״̬��ȡ
    bool IsHovered() const;
    bool IsPressed() const;
    bool IsChecked() const;
    void SetChecked(bool checked);

    // �����Ƿ��ڰ�ť��
    bool ContainsPoint(Vector point) const;
};

#endif
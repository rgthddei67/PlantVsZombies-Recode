#pragma once
#ifndef _INPUTHANDLER_H
#define _INPUTHANDLER_H

#include <SDL.h>
#include <map>
#include <vector>
#include <functional>
#include "../Game/Definit.h"

// ����״̬ö��
enum class KeyState
{
    UP,         // ����δ������
    DOWN,       // ����������
    PRESSED,    // �����ոձ����£���һ֡δ���£�
    RELEASED    // �����ոձ��ͷţ���һ֡���£�
};

class InputHandler
{
private:
    // ��ǰ֡����״̬
    std::map<SDL_Keycode, KeyState> m_keyStates;

    // ��һ֡����״̬
    std::map<SDL_Keycode, KeyState> m_prevKeyStates;

    // ���λ��
    Vector m_mousePosition;

    // ����ƶ�����
    Vector m_mouseDelta;

    // ��갴ť״̬ (0=���, 1=�Ҽ�, 2=�м�, 3=����, 4=ǰ��)
    KeyState m_mouseButtons[5];

    // ��һ֡��갴ť״̬
    KeyState m_prevMouseButtons[5];

    // �����ص���������
    typedef std::function<void()> KeyCallback;

    // �����ص�ӳ��
    std::map<SDL_Keycode, std::vector<KeyCallback>> m_keyCallbacks;

    // ɾ����������͸�ֵ
    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;

public:
    InputHandler();
    ~InputHandler() = default;

    // ����SDL�¼�
    void ProcessEvent(SDL_Event* event);

    // ��������״̬��Ӧ��ÿ֡��ʼʱ���ã�
    void Update();

    // ��ȡ����״̬
    KeyState GetKeyState(SDL_Keycode keyCode) const;

    // ��鰴���Ƿ���
    bool IsKeyDown(SDL_Keycode keyCode) const;

    // ��鰴���Ƿ�ոհ���
    bool IsKeyPressed(SDL_Keycode keyCode) const;

    // ��鰴���Ƿ�ո��ͷ�
    bool IsKeyReleased(SDL_Keycode keyCode) const;

    // ��ȡ���λ��
    Vector GetMousePosition() const;

    // ��ȡ����ƶ�����
    Vector GetMouseDelta() const;

    // ��ȡ��갴ť״̬
    KeyState GetMouseButtonState(Uint8 button) const;

    // �����갴ť�Ƿ���
    bool IsMouseButtonDown(Uint8 button) const;

    // �����갴ť�Ƿ�ոհ���
    bool IsMouseButtonPressed(Uint8 button) const;

    // �����갴ť�Ƿ�ո��ͷ�
    bool IsMouseButtonReleased(Uint8 button) const;

    // ע�ᰴ���ص�
    void RegisterKeyCallback(SDL_Keycode keyCode, KeyCallback callback);

    // �Ƴ������ص�
    void RemoveKeyCallback(SDL_Keycode keyCode, KeyCallback callback);

    // ��������ע��Ļص�
    void ProcessCallbacks();
};

#endif
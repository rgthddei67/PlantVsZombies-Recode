#include "InputHandler.h"
#include <iostream>

InputHandler::InputHandler()
{
    // ��ʼ�����а���״̬ΪUP
    for (int i = 0; i < 5; i++)
    {
        m_mouseButtons[i] = KeyState::UP;
        m_prevMouseButtons[i] = KeyState::UP;
    }

    m_mousePosition = Vector::zero();
    m_mouseDelta = Vector::zero();
}

InputHandler* InputHandler::GetInstance()
{
    if (!s_pInstance)
    {
        s_pInstance = new InputHandler();
        g_pInputHandler = s_pInstance;
    }
    return s_pInstance;
}

void InputHandler::ReleaseInstance()
{
    if (s_pInstance)
    {
        delete s_pInstance;
        s_pInstance = nullptr;
        g_pInputHandler = nullptr;
    }
}

void InputHandler::ProcessEvent(SDL_Event* event)
{
    switch (event->type)
    {
        // �����¼�
    case SDL_KEYDOWN:
    {
        SDL_Keycode key = event->key.keysym.sym;
        // ֻ�ڰ�����ǰ��UP״̬ʱ������ΪPRESSED
        if (m_keyStates[key] == KeyState::UP) {
            m_keyStates[key] = KeyState::PRESSED;
        }
    }
    break;

    case SDL_KEYUP:
    {
        SDL_Keycode key = event->key.keysym.sym;
        // ֻ�ڰ�����ǰ��DOWN��PRESSED״̬ʱ������ΪRELEASED
        if (m_keyStates[key] == KeyState::DOWN || m_keyStates[key] == KeyState::PRESSED) {
            m_keyStates[key] = KeyState::RELEASED;
        }
    }
    break;

    // ����ƶ��¼�
    case SDL_MOUSEMOTION:
        m_mousePosition = Vector(static_cast<float>(event->motion.x),
            static_cast<float>(event->motion.y));
        break;

        // ��갴ť�¼�
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
            int index = event->button.button - 1;
            // ֻ�ڰ�ť��ǰ��UP״̬ʱ������ΪPRESSED
            if (m_mouseButtons[index] == KeyState::UP) {
                m_mouseButtons[index] = KeyState::PRESSED;
            }
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
            int index = event->button.button - 1;
            // ֻ�ڰ�ť��ǰ��DOWN��PRESSED״̬ʱ������ΪRELEASED
            if (m_mouseButtons[index] == KeyState::DOWN || m_mouseButtons[index] == KeyState::PRESSED) {
                m_mouseButtons[index] = KeyState::RELEASED;
            }
        }
        break;

        // �������¼�
    case SDL_MOUSEWHEEL:
        break;
    }
}

void InputHandler::Update()
{
    // ������һ֡�����λ��
    Vector prevMousePos = m_mousePosition;

    // ���浱ǰ״̬����һ֡״̬
    for (int i = 0; i < 5; i++)
    {
        m_prevMouseButtons[i] = m_mouseButtons[i];
    }
    m_prevKeyStates = m_keyStates;

    // ��������ƶ�����
    m_mouseDelta = m_mousePosition - prevMousePos;

    // ����״̬��PRESSED -> DOWN, RELEASED -> UP
    for (auto& pair : m_keyStates)
    {
        if (pair.second == KeyState::PRESSED) {
            pair.second = KeyState::DOWN;
        }
        else if (pair.second == KeyState::RELEASED) {
            pair.second = KeyState::UP;
        }
    }

    for (int i = 0; i < 5; i++)
    {
        if (m_mouseButtons[i] == KeyState::PRESSED) {
            m_mouseButtons[i] = KeyState::DOWN;
        }
        else if (m_mouseButtons[i] == KeyState::RELEASED) {
            m_mouseButtons[i] = KeyState::UP;
        }
    }

    // ��������ע��Ļص�
    ProcessCallbacks();
}

KeyState InputHandler::GetKeyState(SDL_Keycode keyCode) const
{
    auto it = m_keyStates.find(keyCode);
    if (it != m_keyStates.end())
    {
        return it->second;
    }
    return KeyState::UP;
}

bool InputHandler::IsKeyDown(SDL_Keycode keyCode) const
{
    KeyState state = GetKeyState(keyCode);
    return state == KeyState::DOWN || state == KeyState::PRESSED;
}

bool InputHandler::IsKeyPressed(SDL_Keycode keyCode) const
{
    // ֻ���PRESSED״̬
    return GetKeyState(keyCode) == KeyState::PRESSED;
}

bool InputHandler::IsKeyReleased(SDL_Keycode keyCode) const
{
    // ֻ���RELEASED״̬
    return GetKeyState(keyCode) == KeyState::RELEASED;
}

Vector InputHandler::GetMousePosition() const
{
    return m_mousePosition;
}

Vector InputHandler::GetMouseDelta() const
{
    return m_mouseDelta;
}

KeyState InputHandler::GetMouseButtonState(Uint8 button) const
{
    // button��SDL��ťֵ��SDL_BUTTON_LEFT=1, SDL_BUTTON_RIGHT=2, ...��
    if (button >= SDL_BUTTON_LEFT && button <= SDL_BUTTON_X2)
    {
        int index = button - 1; // ת��Ϊ�ڲ�������0-4��
        return m_mouseButtons[index];
    }
    return KeyState::UP;
}

bool InputHandler::IsMouseButtonDown(Uint8 button) const
{
    KeyState state = GetMouseButtonState(button);
    return state == KeyState::DOWN || state == KeyState::PRESSED;
}

bool InputHandler::IsMouseButtonPressed(Uint8 button) const
{
    // ֻ���PRESSED״̬
    return GetMouseButtonState(button) == KeyState::PRESSED;
}

bool InputHandler::IsMouseButtonReleased(Uint8 button) const
{
    // ֻ���RELEASED״̬
    return GetMouseButtonState(button) == KeyState::RELEASED;
}

void InputHandler::RegisterKeyCallback(SDL_Keycode keyCode, KeyCallback callback)
{
    m_keyCallbacks[keyCode].push_back(callback);
}

void InputHandler::RemoveKeyCallback(SDL_Keycode keyCode, KeyCallback callback)
{
    auto it = m_keyCallbacks.find(keyCode);
    if (it != m_keyCallbacks.end())
    {
        auto& callbacks = it->second;
        for (auto cbIt = callbacks.begin(); cbIt != callbacks.end(); )
        {
            if (cbIt->target_type() == callback.target_type())
            {
                cbIt = callbacks.erase(cbIt);
            }
            else
            {
                ++cbIt;
            }
        }
    }
}

void InputHandler::ProcessCallbacks()
{
    for (const auto& pair : m_keyCallbacks)
    {
        SDL_Keycode keyCode = pair.first;
        if (IsKeyPressed(keyCode))
        {
            for (const auto& callback : pair.second)
            {
                callback();
            }
        }
    }
}

bool InputHandler::IsMouseButtonPressedEasyX(Uint8 button) const
{
    if (button >= SDL_BUTTON_LEFT && button <= SDL_BUTTON_X2) {
        int index = button - 1;
        // EasyX���ͨ���Ƚϵ�ǰ֡����һ֡��״̬����ⰴ��
        return m_mouseButtons[index] == KeyState::PRESSED ||
            (m_mouseButtons[index] == KeyState::DOWN &&
                m_prevMouseButtons[index] == KeyState::UP);
    }
    return false;
}

bool InputHandler::IsMouseButtonReleasedEasyX(Uint8 button) const
{
    if (button >= SDL_BUTTON_LEFT && button <= SDL_BUTTON_X2) {
        int index = button - 1;
        // EasyX���ͨ���Ƚϵ�ǰ֡����һ֡��״̬������ͷ�
        return m_mouseButtons[index] == KeyState::RELEASED ||
            (m_mouseButtons[index] == KeyState::UP &&
                m_prevMouseButtons[index] == KeyState::DOWN);
    }
    return false;
}
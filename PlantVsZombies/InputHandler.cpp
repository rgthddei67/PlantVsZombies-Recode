#include "InputHandler.h"
#include <iostream>

InputHandler::InputHandler()
{
    // 初始化所有按键状态为UP
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
        // 键盘事件
    case SDL_KEYDOWN:
        m_keyStates[event->key.keysym.sym] = KeyState::DOWN;
        break;

    case SDL_KEYUP:
        m_keyStates[event->key.keysym.sym] = KeyState::UP;
        break;

        // 鼠标移动事件
    case SDL_MOUSEMOTION:
        m_mousePosition = Vector(static_cast<float>(event->motion.x),
            static_cast<float>(event->motion.y));
        break;

        // 鼠标按钮事件
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
            int index = event->button.button - 1; // SDL_BUTTON_LEFT=1 -> index=0
            m_mouseButtons[index] = KeyState::DOWN;
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
            int index = event->button.button - 1; // SDL_BUTTON_LEFT=1 -> index=0
            m_mouseButtons[index] = KeyState::UP;
        }
        break;

        // 鼠标滚轮事件
    case SDL_MOUSEWHEEL:
        break;
    }
}

void InputHandler::Update()
{
    // 保存上一帧的鼠标位置
    Vector prevMousePos = m_mousePosition;

    // 更新上一帧的状态
    for (int i = 0; i < 5; i++)
    {
        m_prevMouseButtons[i] = m_mouseButtons[i];
    }
    m_prevKeyStates = m_keyStates;

    // 计算鼠标移动增量
    m_mouseDelta = m_mousePosition - prevMousePos;

    // 更新PRESSED和RELEASED状态
    for (auto& pair : m_keyStates)
    {
        SDL_Keycode key = pair.first;
        KeyState currentState = pair.second;

        // 查找上一帧状态
        KeyState prevState = KeyState::UP;
        auto it = m_prevKeyStates.find(key);
        if (it != m_prevKeyStates.end())
        {
            prevState = it->second;
        }

        if (currentState == KeyState::DOWN && prevState == KeyState::UP)
        {
            m_keyStates[key] = KeyState::PRESSED;
        }
        else if (currentState == KeyState::PRESSED)
        {
            // PRESSED状态只持续一帧，下一帧变为DOWN
            m_keyStates[key] = KeyState::DOWN;
        }
        else if (currentState == KeyState::UP && prevState == KeyState::DOWN)
        {
            m_keyStates[key] = KeyState::RELEASED;
        }
        else if (currentState == KeyState::RELEASED)
        {
            // RELEASED状态只持续一帧，下一帧变为UP
            m_keyStates[key] = KeyState::UP;
        }
    }

    // 更新鼠标按钮的PRESSED和RELEASED状态
    for (int i = 0; i < 5; i++)
    {
        // 从UP变为DOWN -> PRESSED
        if (m_mouseButtons[i] == KeyState::DOWN && m_prevMouseButtons[i] == KeyState::UP)
        {
            m_mouseButtons[i] = KeyState::PRESSED;
        }
        // 从DOWN变为UP -> RELEASED  
        else if (m_mouseButtons[i] == KeyState::UP && m_prevMouseButtons[i] == KeyState::DOWN)
        {
            m_mouseButtons[i] = KeyState::RELEASED;
        }
        // PRESSED状态持续一帧后变回DOWN
        else if (m_mouseButtons[i] == KeyState::PRESSED)
        {
            m_mouseButtons[i] = KeyState::DOWN;
        }
        // RELEASED状态持续一帧后变回UP
        else if (m_mouseButtons[i] == KeyState::RELEASED)
        {
            m_mouseButtons[i] = KeyState::UP;
        }
    }

    // 处理所有注册的回调
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
    return GetKeyState(keyCode) == KeyState::PRESSED;
}

bool InputHandler::IsKeyReleased(SDL_Keycode keyCode) const
{
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
    // button是SDL按钮值（SDL_BUTTON_LEFT=1, SDL_BUTTON_RIGHT=2, ...）
    if (button >= SDL_BUTTON_LEFT && button <= SDL_BUTTON_X2) 
    {
        int index = button - 1; // 转换为内部索引（0-4）
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
    return GetMouseButtonState(button) == KeyState::PRESSED;
}

bool InputHandler::IsMouseButtonReleased(Uint8 button) const
{
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
        // EasyX风格：当前帧是DOWN或PRESSED，且上一帧是UP或RELEASED
        return (m_mouseButtons[index] == KeyState::DOWN ||
            m_mouseButtons[index] == KeyState::PRESSED) &&
            (m_prevMouseButtons[index] == KeyState::UP ||
                m_prevMouseButtons[index] == KeyState::RELEASED);
    }
    return false;
}

bool InputHandler::IsMouseButtonReleasedEasyX(Uint8 button) const
{
    if (button >= SDL_BUTTON_LEFT && button <= SDL_BUTTON_X2) {
        int index = button - 1;
        // EasyX风格：当前帧是UP或RELEASED，且上一帧是DOWN或PRESSED
        return (m_mouseButtons[index] == KeyState::UP ||
            m_mouseButtons[index] == KeyState::RELEASED) &&
            (m_prevMouseButtons[index] == KeyState::DOWN ||
                m_prevMouseButtons[index] == KeyState::PRESSED);
    }
    return false;
}
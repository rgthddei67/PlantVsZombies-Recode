#include "InputHandler.h"
#include <iostream>
#include "../GameApp.h"

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

void InputHandler::ProcessEvent(SDL_Event* event)
{
    switch (event->type)
    {
        // 键盘事件
    case SDL_KEYDOWN:
    {
        SDL_Keycode key = event->key.keysym.sym;
        // 只在按键当前是UP状态时才设置为PRESSED
        if (m_keyStates[key] == KeyState::UP) {
            m_keyStates[key] = KeyState::PRESSED;
        }
    }
    break;

    case SDL_KEYUP:
    {
        SDL_Keycode key = event->key.keysym.sym;
        // 只在按键当前是DOWN或PRESSED状态时才设置为RELEASED
        if (m_keyStates[key] == KeyState::DOWN || m_keyStates[key] == KeyState::PRESSED) {
            m_keyStates[key] = KeyState::RELEASED;
        }
    }
    break;

    // 鼠标移动事件
    case SDL_MOUSEMOTION:
        m_mousePosition = Vector(static_cast<float>(event->motion.x),
            static_cast<float>(event->motion.y));
        break;

        // 鼠标按钮事件
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
            int index = event->button.button - 1;
            // 只在按钮当前是UP状态时才设置为PRESSED
            if (m_mouseButtons[index] == KeyState::UP) {
                m_mouseButtons[index] = KeyState::PRESSED;
            }
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
            int index = event->button.button - 1;
            // 只在按钮当前是DOWN或PRESSED状态时才设置为RELEASED
            if (m_mouseButtons[index] == KeyState::DOWN || m_mouseButtons[index] == KeyState::PRESSED) {
                m_mouseButtons[index] = KeyState::RELEASED;
            }
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

    // 保存当前状态到上一帧状态
    for (int i = 0; i < 5; i++)
    {
        m_prevMouseButtons[i] = m_mouseButtons[i];
    }
    m_prevKeyStates = m_keyStates;

    // 计算鼠标移动增量
    m_mouseDelta = m_mousePosition - prevMousePos;

    // 更新状态：PRESSED -> DOWN, RELEASED -> UP
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
    // 只检测PRESSED状态
    return GetKeyState(keyCode) == KeyState::PRESSED;
}

bool InputHandler::IsKeyReleased(SDL_Keycode keyCode) const
{
    // 只检测RELEASED状态
    return GetKeyState(keyCode) == KeyState::RELEASED;
}

Vector InputHandler::GetMousePosition() const
{
    return m_mousePosition;
}

Vector InputHandler::GetMouseWorldPosition() const {
    Vector mousePositon = GetMousePosition();
    return GameAPP::GetInstance().GetCamera().ScreenToWorld
    ({ mousePositon.x, mousePositon.y });
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
    // 只检测PRESSED状态
    return GetMouseButtonState(button) == KeyState::PRESSED;
}

bool InputHandler::IsMouseButtonReleased(Uint8 button) const
{
    // 只检测RELEASED状态
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
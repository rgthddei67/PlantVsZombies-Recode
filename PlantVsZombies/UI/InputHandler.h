#pragma once
#ifndef _INPUTHANDLER_H
#define _INPUTHANDLER_H

#include <SDL2/SDL.h>
#include <map>
#include <vector>
#include <functional>
#include "../Game/Definit.h"

class Graphics;

enum class KeyState
{
    UP,        
    DOWN,       
    PRESSED,    // 按下
    RELEASED    // 按键松开
};

class InputHandler
{
private:
    Graphics* mGraphics = nullptr;

    std::map<SDL_Keycode, KeyState> m_keyStates;

    std::map<SDL_Keycode, KeyState> m_prevKeyStates;

    Vector m_mousePosition;

    Vector m_mouseDelta;

    KeyState m_mouseButtons[5];

    KeyState m_prevMouseButtons[5];

    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;

public:
    InputHandler(Graphics* graphics);
    ~InputHandler() = default;

    void ProcessEvent(SDL_Event* event);

    void Update();

    KeyState GetKeyState(SDL_Keycode keyCode) const;

    bool IsKeyDown(SDL_Keycode keyCode) const;

    bool IsKeyPressed(SDL_Keycode keyCode) const;

    bool IsKeyReleased(SDL_Keycode keyCode) const;

    Vector GetMousePosition() const;

    Vector GetMouseWorldPosition() const;

    Vector GetMouseDelta() const;

    KeyState GetMouseButtonState(Uint8 button) const;

    bool IsMouseButtonDown(Uint8 button) const;

    bool IsMouseButtonPressed(Uint8 button) const;

    bool IsMouseButtonReleased(Uint8 button) const;
};

#endif
#pragma once
#ifndef _INPUTHANDLER_H
#define _INPUTHANDLER_H

#include <SDL2/SDL.h>
#include <map>
#include <vector>
#include <functional>
#include "../Game/Definit.h"

// 按键状态枚举
enum class KeyState
{
    UP,         // 按键未被按下
    DOWN,       // 按键被按下
    PRESSED,    // 按键刚刚被按下（上一帧未按下）
    RELEASED    // 按键刚刚被释放（上一帧按下）
};

class InputHandler
{
private:
    // 当前帧按键状态
    std::map<SDL_Keycode, KeyState> m_keyStates;

    // 上一帧按键状态
    std::map<SDL_Keycode, KeyState> m_prevKeyStates;

    // 鼠标位置
    Vector m_mousePosition;

    // 鼠标移动增量
    Vector m_mouseDelta;

    // 鼠标按钮状态 (0=左键, 1=右键, 2=中键, 3=后退, 4=前进)
    KeyState m_mouseButtons[5];

    // 上一帧鼠标按钮状态
    KeyState m_prevMouseButtons[5];

    // 按键回调函数类型
    typedef std::function<void()> KeyCallback;

    // 按键回调映射
    std::map<SDL_Keycode, std::vector<KeyCallback>> m_keyCallbacks;

    // 删除拷贝构造和赋值
    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;

public:
    InputHandler();
    ~InputHandler() = default;

    // 处理SDL事件
    void ProcessEvent(SDL_Event* event);

    // 更新输入状态（应在每帧开始时调用）
    void Update();

    // 获取按键状态
    KeyState GetKeyState(SDL_Keycode keyCode) const;

    // 检查按键是否按下
    bool IsKeyDown(SDL_Keycode keyCode) const;

    // 检查按键是否刚刚按下
    bool IsKeyPressed(SDL_Keycode keyCode) const;

    // 检查按键是否刚刚释放
    bool IsKeyReleased(SDL_Keycode keyCode) const;

    // 获取鼠标屏幕坐标
    Vector GetMousePosition() const;

    // 获取鼠标世界坐标
    Vector GetMouseWorldPosition() const;

    // 获取鼠标移动增量
    Vector GetMouseDelta() const;

    // 获取鼠标按钮状态
    KeyState GetMouseButtonState(Uint8 button) const;

    // 检查鼠标按钮是否按下
    bool IsMouseButtonDown(Uint8 button) const;

    // 检查鼠标按钮是否刚刚按下
    bool IsMouseButtonPressed(Uint8 button) const;

    // 检查鼠标按钮是否刚刚释放
    bool IsMouseButtonReleased(Uint8 button) const;

    // 注册按键回调
    void RegisterKeyCallback(SDL_Keycode keyCode, KeyCallback callback);

    // 移除按键回调
    void RemoveKeyCallback(SDL_Keycode keyCode, KeyCallback callback);

    // 处理所有注册的回调
    void ProcessCallbacks();
};

#endif
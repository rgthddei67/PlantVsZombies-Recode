#pragma once
#ifndef _INPUTHANDLER_H
#define _INPUTHANDLER_H

#include <SDL2/SDL.h>
#include <map>
#include <vector>
#include <functional>
#include "../Game/Definit.h"

class Graphics;

// 鎸夐敭鐘舵€佹灇涓?
enum class KeyState
{
    UP,         // 鎸夐敭鏈鎸変笅
    DOWN,       // 鎸夐敭琚寜涓?
    PRESSED,    // 鎸夐敭鍒氬垰琚寜涓嬶紙涓婁竴甯ф湭鎸変笅锛?
    RELEASED    // 鎸夐敭鍒氬垰琚噴鏀撅紙涓婁竴甯ф寜涓嬶級
};

class InputHandler
{
private:
    Graphics* mGraphics = nullptr;

    // 褰撳墠甯ф寜閿姸鎬?
    std::map<SDL_Keycode, KeyState> m_keyStates;

    // 涓婁竴甯ф寜閿姸鎬?
    std::map<SDL_Keycode, KeyState> m_prevKeyStates;

    // 榧犳爣浣嶇疆
    Vector m_mousePosition;

    // 榧犳爣绉诲姩澧為噺
    Vector m_mouseDelta;

    // 榧犳爣鎸夐挳鐘舵€?(0=宸﹂敭, 1=鍙抽敭, 2=涓敭, 3=鍚庨€€, 4=鍓嶈繘)
    KeyState m_mouseButtons[5];

    // 涓婁竴甯ч紶鏍囨寜閽姸鎬?
    KeyState m_prevMouseButtons[5];

    // 鎸夐敭鍥炶皟鍑芥暟绫诲瀷
    typedef std::function<void()> KeyCallback;

    // 鎸夐敭鍥炶皟鏄犲皠
    std::map<SDL_Keycode, std::vector<KeyCallback>> m_keyCallbacks;

    // 鍒犻櫎鎷疯礉鏋勯€犲拰璧嬪€?
    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;

public:
    InputHandler(Graphics* graphics);
    ~InputHandler() = default;

    // 澶勭悊SDL浜嬩欢
    void ProcessEvent(SDL_Event* event);

    // 鏇存柊杈撳叆鐘舵€侊紙搴斿湪姣忓抚寮€濮嬫椂璋冪敤锛?
    void Update();

    // 鑾峰彇鎸夐敭鐘舵€?
    KeyState GetKeyState(SDL_Keycode keyCode) const;

    // 妫€鏌ユ寜閿槸鍚︽寜涓?
    bool IsKeyDown(SDL_Keycode keyCode) const;

    // 妫€鏌ユ寜閿槸鍚﹀垰鍒氭寜涓?
    bool IsKeyPressed(SDL_Keycode keyCode) const;

    // 妫€鏌ユ寜閿槸鍚﹀垰鍒氶噴鏀?
    bool IsKeyReleased(SDL_Keycode keyCode) const;

    // 鑾峰彇榧犳爣灞忓箷鍧愭爣
    Vector GetMousePosition() const;

    // 鑾峰彇榧犳爣涓栫晫鍧愭爣
    Vector GetMouseWorldPosition() const;

    // 鑾峰彇榧犳爣绉诲姩澧為噺
    Vector GetMouseDelta() const;

    // 鑾峰彇榧犳爣鎸夐挳鐘舵€?
    KeyState GetMouseButtonState(Uint8 button) const;

    // 妫€鏌ラ紶鏍囨寜閽槸鍚︽寜涓?
    bool IsMouseButtonDown(Uint8 button) const;

    // 妫€鏌ラ紶鏍囨寜閽槸鍚﹀垰鍒氭寜涓?
    bool IsMouseButtonPressed(Uint8 button) const;

    // 妫€鏌ラ紶鏍囨寜閽槸鍚﹀垰鍒氶噴鏀?
    bool IsMouseButtonReleased(Uint8 button) const;

    // 娉ㄥ唽鎸夐敭鍥炶皟
    void RegisterKeyCallback(SDL_Keycode keyCode, KeyCallback callback);

    // 绉婚櫎鎸夐敭鍥炶皟
    void RemoveKeyCallback(SDL_Keycode keyCode, KeyCallback callback);

    // 澶勭悊鎵€鏈夋敞鍐岀殑鍥炶皟
    void ProcessCallbacks();
};

#endif
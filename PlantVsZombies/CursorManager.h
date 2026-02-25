#pragma once
#ifndef _CURSOR_MANAGER_H
#define _CURSOR_MANAGER_H

#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>

enum class CursorType {
    ARROW,      // 默认箭头
    HAND,       // 手型（可点击）
    IBEAM,      // I型（文本输入）
    CROSSHAIR,  // 十字准星
    WAIT,       // 等待
    SIZE_ALL,   // 所有方向调整
    NO,         // 禁止操作
    CUSTOM      // 自定义光标
};

class CursorManager {
private:
    SDL_Cursor* mCurrentCursor = nullptr;
    std::unordered_map<CursorType, SDL_Cursor*> mSystemCursors;
    std::unordered_map<std::string, SDL_Cursor*> mCustomCursors;

    bool mIsInitialized = false;
    bool mCursorHidden = false;

    int m_hoverCount = 0;
    bool m_forceUpdate = false;

    CursorManager();
    ~CursorManager();

    CursorManager(const CursorManager&) = delete;
    CursorManager& operator=(const CursorManager&) = delete;

    // 内部辅助方法
    void InitializeSystemCursors();
    void CleanupAllCursors();
    SDL_Cursor* GetSystemCursor(CursorType type);

public:
    static CursorManager& GetInstance();

    // 初始化与清理
    bool Initialize();
    void Cleanup();

    // 光标设置
    bool SetCursor(CursorType type);
    bool SetCustomCursor(const std::string& cursorName);
    bool SetDefaultCursor();

    // 自定义光标管理
    bool LoadCustomCursor(const std::string& name, const std::string& imagePath,
        int hotX = 0, int hotY = 0);
    bool LoadCustomCursorFromSurface(const std::string& name, SDL_Surface* surface,
        int hotX = 0, int hotY = 0);
    bool RemoveCustomCursor(const std::string& name);
    
    void Update();

    void IncrementHoverCount() {
        m_hoverCount++;
        m_forceUpdate = true; // 标记需要更新光标
    }

    void DecrementHoverCount() {
        if (m_hoverCount > 0) {
            m_hoverCount--;
        }
        m_forceUpdate = true; 
    }

    void ResetHoverCount() {
        m_hoverCount = 0;
        m_forceUpdate = true;
    }

    int GetHoverCount() const { return m_hoverCount; }

    // 光标可见性控制
    void ShowCursor();
    void HideCursor();
    bool IsCursorVisible() const { return !mCursorHidden; }

    // 状态查询
    CursorType GetCurrentCursorType() const;
    bool IsInitialized() const { return mIsInitialized; }

    // 自动悬停检测
    void ForceUpdateCursor();
};

#endif
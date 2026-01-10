#include "CursorManager.h"
#include <iostream>

CursorManager::CursorManager() {
    // 初始化系统光标映射
    mSystemCursors[CursorType::ARROW] = nullptr;
    mSystemCursors[CursorType::HAND] = nullptr;
    mSystemCursors[CursorType::IBEAM] = nullptr;
    mSystemCursors[CursorType::CROSSHAIR] = nullptr;
    mSystemCursors[CursorType::WAIT] = nullptr;
    mSystemCursors[CursorType::SIZE_ALL] = nullptr;
    mSystemCursors[CursorType::NO] = nullptr;
}

CursorManager::~CursorManager() {
    Cleanup();
}

CursorManager& CursorManager::GetInstance() {
    static CursorManager instance;
    return instance;
}

bool CursorManager::Initialize() {
    if (mIsInitialized) return true;

    // 初始化系统光标
    InitializeSystemCursors();

    mIsInitialized = true;
    SetDefaultCursor();

#ifdef _DEBUG
    std::cout << "CursorManager initialized successfully." << std::endl;
#endif

    return true;
}

void CursorManager::InitializeSystemCursors() {
    mSystemCursors[CursorType::ARROW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    mSystemCursors[CursorType::HAND] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    mSystemCursors[CursorType::IBEAM] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
    mSystemCursors[CursorType::CROSSHAIR] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    mSystemCursors[CursorType::WAIT] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
    mSystemCursors[CursorType::SIZE_ALL] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    mSystemCursors[CursorType::NO] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);

    // 检查是否都创建成功
    for (auto& [type, cursor] : mSystemCursors) {
        if (!cursor) {
            std::cerr << "Failed to create system cursor for type: " << static_cast<int>(type) << std::endl;
        }
    }
}

void CursorManager::Cleanup() {
    if (!mIsInitialized) return;

    CleanupAllCursors();
    mIsInitialized = false;
    mCurrentCursor = nullptr;
}

void CursorManager::CleanupAllCursors() {
    // 清理系统光标
    for (auto& [type, cursor] : mSystemCursors) {
        if (cursor) {
            SDL_FreeCursor(cursor);
            cursor = nullptr;
        }
    }

    // 清理自定义光标
    for (auto& [name, cursor] : mCustomCursors) {
        if (cursor) {
            SDL_FreeCursor(cursor);
        }
    }
    mCustomCursors.clear();
}

bool CursorManager::SetCursor(CursorType type) {
    if (!mIsInitialized) return false;

    SDL_Cursor* newCursor = GetSystemCursor(type);
    if (!newCursor) {
        std::cerr << "Cursor type not available: " << static_cast<int>(type) << std::endl;
        return false;
    }

    if (newCursor != mCurrentCursor) {
        SDL_SetCursor(newCursor);
        mCurrentCursor = newCursor;
        return true;
    }

    return false;
}

bool CursorManager::SetCustomCursor(const std::string& cursorName) {
    if (!mIsInitialized) return false;

    auto it = mCustomCursors.find(cursorName);
    if (it == mCustomCursors.end()) {
        std::cerr << "Custom cursor not found: " << cursorName << std::endl;
        return false;
    }

    SDL_Cursor* newCursor = it->second;
    if (newCursor && newCursor != mCurrentCursor) {
        SDL_SetCursor(newCursor);
        mCurrentCursor = newCursor;
        return true;
    }

    return false;
}

bool CursorManager::SetDefaultCursor() {
    return SetCursor(CursorType::ARROW);
}

SDL_Cursor* CursorManager::GetSystemCursor(CursorType type) {
    auto it = mSystemCursors.find(type);
    return (it != mSystemCursors.end()) ? it->second : nullptr;
}

bool CursorManager::LoadCustomCursor(const std::string& name,
    const std::string& imagePath,
    int hotX, int hotY) {
    // 加载图像
    SDL_Surface* surface = SDL_LoadBMP(imagePath.c_str());
    if (!surface) {
        std::cerr << "Failed to load cursor image: " << imagePath
            << " - " << SDL_GetError() << std::endl;
        return false;
    }

    return LoadCustomCursorFromSurface(name, surface, hotX, hotY);
}

bool CursorManager::LoadCustomCursorFromSurface(const std::string& name,
    SDL_Surface* surface,
    int hotX, int hotY) {
    if (!surface) return false;

    // 确保表面格式正确（需要单色位图）
    // SDL_CreateCursor需要位图数据，这里简化处理
    // 实际项目中可能需要转换表面格式

    // 创建位图数据（简化示例）
    int width = surface->w;
    int height = surface->h;

    // 将表面转换为单色位图（实际实现会更复杂）
    // 这里只是示例，实际需要正确处理alpha通道和颜色键

    // 创建光标（实际应该根据表面数据创建）
    SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX, hotY);

    if (!cursor) {
        std::cerr << "Failed to create custom cursor: " << name
            << " - " << SDL_GetError() << std::endl;
        return false;
    }

    // 存储光标
    RemoveCustomCursor(name); // 如果已存在，先移除
    mCustomCursors[name] = cursor;

    return true;
}

bool CursorManager::RemoveCustomCursor(const std::string& name) {
    auto it = mCustomCursors.find(name);
    if (it != mCustomCursors.end()) {
        if (mCurrentCursor == it->second) {
            SetDefaultCursor();
        }
        SDL_FreeCursor(it->second);
        mCustomCursors.erase(it);
        return true;
    }
    return false;
}

void CursorManager::ShowCursor() {
    if (mCursorHidden) {
        SDL_ShowCursor(SDL_ENABLE);
        mCursorHidden = false;
    }
}

void CursorManager::HideCursor() {
    if (!mCursorHidden) {
        SDL_ShowCursor(SDL_DISABLE);
        mCursorHidden = true;
    }
}

CursorType CursorManager::GetCurrentCursorType() const {
    // 查找当前光标对应的类型
    for (const auto& [type, cursor] : mSystemCursors) {
        if (cursor == mCurrentCursor) {
            return type;
        }
    }

    // 检查是否为自定义光标
    for (const auto& [name, cursor] : mCustomCursors) {
        if (cursor == mCurrentCursor) {
            return CursorType::CUSTOM;
        }
    }

    return CursorType::ARROW; // 默认
}

void CursorManager::Update() {
    if (m_forceUpdate) {
        if (m_hoverCount > 0) {
            SetCursor(CursorType::HAND);
        }
        else {
            SetDefaultCursor();
        }
        m_forceUpdate = false;
    }
}

void CursorManager::ForceUpdateCursor() {
    if (mCurrentCursor) {
        SDL_SetCursor(mCurrentCursor);
    }
}
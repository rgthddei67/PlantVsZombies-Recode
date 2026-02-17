#pragma once
#ifndef _GAMEAPP_H
#define _GAMEAPP_H
#ifdef DrawText
#undef DrawText
#endif
#include "ResourceKeys.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <stdexcept>
#include "./Game/Definit.h"
#include "./ParticleSystem/ParticleSystem.h"

struct TextCache {
    std::string key;        // 缓存键（字体+颜色等）
    SDL_Texture* texture;   // 缓存的纹理
    int width;              // 纹理宽度
    int height;             // 纹理高度
};

class InputHandler;

class GameAPP
{
public:
    int Difficulty = 1; // 难度系数

private:
    std::unique_ptr<InputHandler> mInputHandler;
    std::vector<TextCache> mTextCache;

    SDL_Window* mWindow;
    SDL_Renderer* mRenderer;

    // 游戏运行状态
    bool mRunning;
    bool mInitialized;

    GameAPP();
    ~GameAPP();

    GameAPP(const GameAPP&) = delete;
    GameAPP& operator=(const GameAPP&) = delete;

    bool InitializeSDL();
    bool InitializeSDL_Image();
    bool InitializeSDL_TTF();
    bool InitializeAudioSystem();
    bool CreateWindowAndRenderer();
    bool InitializeResourceManager();
    bool LoadAllResources();
    void CleanupResources();
    void Update();
    void Draw();
    void Shutdown();

    SDL_Texture* GetCachedTextTexture(const std::string& text,
        const SDL_Color& color,
        const std::string& fontKey,
        int fontSize,
        int& outWidth,
        int& outHeight);    // 获取保存的Texture

public:
    inline static bool mDebugMode = false;        // 是否是调试模式
    inline static bool mShowColliders = false;    // 显示碰撞框开关

    // 获取单例实例
    static GameAPP& GetInstance();

    int Run();

    bool Initialize();

    // 清除文本缓存
    void ClearTextCache();

    // 绘制文本 UTF8编码
    void DrawText(const std::string& text,
        int x, int y,
        const SDL_Color& color,
        const std::string& fontKey = ResourceKeys::Fonts::FONT_FZCQ,
        int fontSize = 17);

    // 绘制文本 UTF8编码
    void DrawText(const std::string& text,
        const Vector& position,
        const SDL_Color& color,
        const std::string& fontKey = ResourceKeys::Fonts::FONT_FZCQ,
        int fontSize = 17);

    // 文本尺寸
    Vector GetTextSize(const std::string& text,
        const std::string& fontKey = ResourceKeys::Fonts::FONT_FZCQ,
        int fontSize = 17);

    // 创建文本纹理对象
    SDL_Texture* CreateTextTexture(SDL_Renderer* renderer,
        const std::string& text,
        const SDL_Color& color,
        const std::string& fontKey = ResourceKeys::Fonts::FONT_FZCQ,
        int fontSize = 17);

    // 获取输入处理器
    InputHandler& GetInputHandler() const {
        if (!mInputHandler) {
            throw std::runtime_error("InputHandler not initialized");
        }
        return *mInputHandler;
    }

    // 检查输入处理器是否有效
    bool IsInputHandlerValid() const { return mInputHandler != nullptr; }

    // 获取渲染器
    SDL_Renderer* GetRenderer() const { return mRenderer; }

    // 获取窗口
    SDL_Window* GetWindow() const { return mWindow; }

};

#endif
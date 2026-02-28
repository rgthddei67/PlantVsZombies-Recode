#pragma once
#ifndef _GAMEAPP_H
#define _GAMEAPP_H
#ifdef DrawText
#undef DrawText
#endif
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
#include "ResourceKeys.h"
#include "./Game/Definit.h"
#include "./ParticleSystem/ParticleSystem.h"
#include "GameInfoSaver.h"
#include "./Game/Plant/PlantType.h"
#include "Graphics.h"      

constexpr int SCENE_WIDTH = 1100;
constexpr int SCENE_HEIGHT = 600;

class InputHandler;

class GameAPP
{
public:
    int Difficulty = 1; // 难度系数
    int mAdventureLevel = 1;    // 玩到的冒险模式关卡
    bool mShowPlantHP = false;  // 植物显示血量
    bool mShowZombieHP = false; // 僵尸显示血量
    bool mAutoCollected = true; // 自动收集

    std::vector<PlantType> mHaveCards;      // 玩家拥有的卡牌

    GameInfoSaver mGameInfoSaver;

private:
    std::unique_ptr<InputHandler> mInputHandler;
    std::unique_ptr<Graphics> m_graphics;   // 改用 Graphics

    SDL_Window* mWindow;
    SDL_GLContext m_glContext;               // OpenGL 上下文

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
    void Draw(Uint64 start);
    void Shutdown();

public:
    inline static bool mDebugMode = false;        // 是否是调试模式
    inline static bool mShowColliders = false;    // 显示碰撞框开关

    static GameAPP& GetInstance();

    int Run();
    bool Initialize();

    // 获取 Graphics 对象
    Graphics& GetGraphics() { return *m_graphics; }

    // 设置游戏是否运行
    void SetRunning(bool running) { this->mRunning = running; }

    // 世界坐标绘制文本 UTF8编码
    void DrawText(const std::string& text,
        const Vector& position,
        const glm::vec4& color,
        const std::string& fontKey = ResourceKeys::Fonts::FONT_FZCQ,
        int fontSize = 17);

    // 获取输入处理器
    InputHandler& GetInputHandler() const {
        if (!mInputHandler) {
            throw std::runtime_error("InputHandler not initialized");
        }
        return *mInputHandler;
    }

    bool IsInputHandlerValid() const { return mInputHandler != nullptr; }

    // 获取窗口 (可能用于其他目的)
    SDL_Window* GetWindow() const { return mWindow; }

};

#endif
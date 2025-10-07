#pragma once
#ifndef _GAMEAPP_H
#define _GAMEAPP_H

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include "./Game/Definit.h"
#include "./Game/Board.h"

struct TextCache
{
    std::string text;
    SDL_Color color;
    std::string fontKey;
    int fontSize;
    SDL_Texture* texture;
    int width;
    int height;
};

class InputHandler;

class Board;

class GameAPP
{
private:
    std::unique_ptr<Board> mBoard;
    std::unique_ptr<InputHandler> mInputHandler;
    std::vector<TextCache> mTextCache;

    GameAPP();
    ~GameAPP();

    // 删除拷贝构造和赋值
    GameAPP(const GameAPP&) = delete;
    GameAPP& operator=(const GameAPP&) = delete;

public:
    // 获取单例实例
    static GameAPP& GetInstance();

    // 初始化游戏应用
    bool Initialize();

    // 关闭游戏
    void CloseGame();

    // 创建新Board
    void CreateNewBoard();

    // 清除文本缓存
    void ClearTextCache();

    // 清理资源
    void CleanupResources();

    // 绘制文本 使用UTF8编码字符串
    void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        int x, int y,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // 绘制文本 使用UTF8编码字符串
    void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        const Vector& position,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // 文本尺寸
    Vector GetTextSize(const std::string& text,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // 创建文本纹理对象
    SDL_Texture* CreateTextTexture(SDL_Renderer* renderer,
        const std::string& text,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
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
};

#endif
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
#include <functional>
#include "Definit.h"

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

// 前向声明
class InputHandler;

// 单例模式访问点
extern InputHandler* g_pInputHandler;

class InputHandler; // 前向声明

class GameAPP
{
public:
    // 关闭游戏
    void CloseGame();
    // 文本缓存清理
    static void ClearTextCache();
    // 资源清理方法
    static void CleanupResources();

    // 绘制文本 中文字符前面加u8
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        int x, int y,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf", 
        int fontSize = 17);

    // 绘制文本 中文字符前面加u8
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        const Vector& position,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // 文本测量
    static Vector GetTextSize(const std::string& text,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // 添加文本缓存优化
    static SDL_Texture* CreateTextTexture(SDL_Renderer* renderer,
        const std::string& text,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);
};

#endif
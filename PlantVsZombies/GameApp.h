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

// 前向声明
class InputHandler;

// 单例模式访问点
extern InputHandler* g_pInputHandler;

class InputHandler; // 前向声明

class GameAPP
{
private:
    std::vector<SDL_Texture*> gameTextures;    // SDL纹理集合
    std::vector<std::string> imagePaths =
    {
        "./resources/image/kid.jpg",
        "./resources/image/UI/options_checkbox0.png",
        "./resources/image/UI/options_checkbox1.png",
        "./resources/image/UI/SeedChooser_Button2.png",
        "./resources/image/UI/SeedChooser_Button2_Glow.png",
    };

public:
    // 加载游戏资源
    bool LoadGameResources(SDL_Renderer* renderer);

    // 关闭游戏
    void CloseGame(SDL_Renderer* renderer, SDL_Window* window);

    // 获取纹理资源
    const std::vector<SDL_Texture*>& GetGameTextures() const;

    // 清理纹理资源
    void ClearTextures();
    
    // 绘制文字
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        int x, int y,
        const SDL_Color& color,
        const std::string& fontName = "./font/fzcq.ttf",
        int fontSize = 17);
    // 绘制文字
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        const Vector& position,
        const SDL_Color& color,
        const std::string& fontName = "./font/fzcq.ttf",
        int fontSize = 17);
};

#endif
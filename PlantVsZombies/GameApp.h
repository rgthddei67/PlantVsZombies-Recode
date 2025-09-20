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

// ǰ������
class InputHandler;

// ����ģʽ���ʵ�
extern InputHandler* g_pInputHandler;

class InputHandler; // ǰ������

class GameAPP
{
private:
    std::vector<SDL_Texture*> gameTextures;    // SDL������
    std::vector<std::string> imagePaths =
    {
        "./resources/image/kid.jpg",
        "./resources/image/UI/options_checkbox0.png",
        "./resources/image/UI/options_checkbox1.png",
        "./resources/image/UI/SeedChooser_Button2.png",
        "./resources/image/UI/SeedChooser_Button2_Glow.png",
    };

public:
    // ������Ϸ��Դ
    bool LoadGameResources(SDL_Renderer* renderer);

    // �ر���Ϸ
    void CloseGame(SDL_Renderer* renderer, SDL_Window* window);

    // ��ȡ������Դ
    const std::vector<SDL_Texture*>& GetGameTextures() const;

    // ����������Դ
    void ClearTextures();
    
    // ��������
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        int x, int y,
        const SDL_Color& color,
        const std::string& fontName = "./font/fzcq.ttf",
        int fontSize = 17);
    // ��������
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        const Vector& position,
        const SDL_Color& color,
        const std::string& fontName = "./font/fzcq.ttf",
        int fontSize = 17);
};

#endif
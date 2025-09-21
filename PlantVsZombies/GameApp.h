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

// ǰ������
class InputHandler;

// ����ģʽ���ʵ�
extern InputHandler* g_pInputHandler;

class InputHandler; // ǰ������

class GameAPP
{
public:
    // �ر���Ϸ
    void CloseGame();
    // �ı���������
    static void ClearTextCache();
    // ��Դ������
    static void CleanupResources();

    // �����ı� �����ַ�ǰ���u8
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        int x, int y,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf", 
        int fontSize = 17);

    // �����ı� �����ַ�ǰ���u8
    static void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        const Vector& position,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // �ı�����
    static Vector GetTextSize(const std::string& text,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // ����ı������Ż�
    static SDL_Texture* CreateTextTexture(SDL_Renderer* renderer,
        const std::string& text,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);
};

#endif
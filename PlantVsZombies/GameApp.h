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

    // ɾ����������͸�ֵ
    GameAPP(const GameAPP&) = delete;
    GameAPP& operator=(const GameAPP&) = delete;

public:
    // ��ȡ����ʵ��
    static GameAPP& GetInstance();

    // ��ʼ����ϷӦ��
    bool Initialize();

    // �ر���Ϸ
    void CloseGame();

    // ������Board
    void CreateNewBoard();

    // ����ı�����
    void ClearTextCache();

    // ������Դ
    void CleanupResources();

    // �����ı� ʹ��UTF8�����ַ���
    void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        int x, int y,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // �����ı� ʹ��UTF8�����ַ���
    void DrawText(SDL_Renderer* renderer,
        const std::string& text,
        const Vector& position,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // �ı��ߴ�
    Vector GetTextSize(const std::string& text,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // �����ı��������
    SDL_Texture* CreateTextTexture(SDL_Renderer* renderer,
        const std::string& text,
        const SDL_Color& color,
        const std::string& fontKey = "./font/fzcq.ttf",
        int fontSize = 17);

    // ��ȡ���봦����
    InputHandler& GetInputHandler() const {
        if (!mInputHandler) {
            throw std::runtime_error("InputHandler not initialized");
        }
        return *mInputHandler;
    }

	// ������봦�����Ƿ���Ч
    bool IsInputHandlerValid() const { return mInputHandler != nullptr; }
};

#endif
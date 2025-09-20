#include "GameApp.h"
#include "InputHandler.h"
#include <iostream>
#include <sstream>

InputHandler* InputHandler::s_pInstance = nullptr;
InputHandler* g_pInputHandler = nullptr;

bool GameAPP::LoadGameResources(SDL_Renderer* renderer)
{
    if (!renderer)
    {
        std::cerr << "��Ⱦ����Ч���޷�������Դ" << std::endl;
        return false;
    }

    // ��������������
    ClearTextures();

    for (const auto& path : imagePaths)
    {
        // ʹ��SDL_image��������
        SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
        if (!texture)
        {
            std::string errorMsg = "��������ʧ��: " + path + " - " + IMG_GetError();
            std::cerr << errorMsg << std::endl;

            // �����Ѽ��ص�����
            ClearTextures();
            return false;
        }

        gameTextures.push_back(texture);
        std::cout << "�ɹ���������: " << path << std::endl;
    }

    std::cout << "��Դ�������! ������ " << gameTextures.size()
        << "/" << imagePaths.size() << " ������" << std::endl;
    return true;
}

void GameAPP::CloseGame(SDL_Renderer* renderer, SDL_Window* window)
{
    // ����������Դ
    ClearTextures();

    // �������봦����
    if (g_pInputHandler != nullptr)
    {
        InputHandler::ReleaseInstance();
    }

    // ֻ����Ӧ����ص���Դ
}

const std::vector<SDL_Texture*>& GameAPP::GetGameTextures() const
{
    return gameTextures;
}

void GameAPP::ClearTextures()
{
    for (auto texture : gameTextures)
    {
        if (texture)
        {
            SDL_DestroyTexture(texture);
        }
    }
    gameTextures.clear();
}

void GameAPP::DrawText(SDL_Renderer* renderer,
    const std::string& text,
    int x, int y,
    const SDL_Color& color,
    const std::string& fontName,
    int fontSize)
{
    // ��������
    TTF_Font* font = TTF_OpenFont(fontName.c_str(), fontSize);
    if (!font)
    {
        std::cerr << "�޷���������: " << TTF_GetError() << std::endl;
        return;
    }

    // �����ı�����
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface)
    {
        TTF_CloseFont(font);
        std::cerr << "�޷������ı�����: " << TTF_GetError() << std::endl;
        return;
    }

    // ��������
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_FreeSurface(surface);
        TTF_CloseFont(font);
        std::cerr << "�޷���������: " << SDL_GetError() << std::endl;
        return;
    }

    // ��Ⱦ�ı�
    SDL_Rect destRect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);

    // ������Դ
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    TTF_CloseFont(font);
}

void GameAPP::DrawText(SDL_Renderer* renderer,
    const std::string& text,
    const Vector& position,
    const SDL_Color& color,
    const std::string& fontName,
    int fontSize)
{
    DrawText(renderer, text, static_cast<int>(position.x), static_cast<int>(position.y),
        color, fontName, fontSize);
}
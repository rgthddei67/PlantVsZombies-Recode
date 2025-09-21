#include "GameApp.h"
#include "InputHandler.h"
#include "ResourceManager.h"
#include <iostream>
#include <sstream>

InputHandler* InputHandler::s_pInstance = nullptr;
InputHandler* g_pInputHandler = nullptr;

void GameAPP::CloseGame()
{
    if (g_pInputHandler != nullptr)
    {
        InputHandler::ReleaseInstance();
    }
}

static std::vector<TextCache> textCache;

void GameAPP::ClearTextCache()
{
    for (auto& cache : textCache)
    {
        if (cache.texture)
        {
            SDL_DestroyTexture(cache.texture);
        }
    }
    textCache.clear();
    std::cout << "�����ı�����" << std::endl;
}

void GameAPP::CleanupResources()
{
    ClearTextCache();
    ResourceManager::GetInstance().CleanupUnusedFontSizes();
}

void GameAPP::DrawText(SDL_Renderer* renderer,
    const std::string& text,
    int x, int y,
    const SDL_Color& color,
    const std::string& fontKey,
    int fontSize)
{
    if (text.empty()) return;

    // ʹ����Դ��������ȡ����
    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font)
    {
        std::cerr << "�޷���ȡ����: " << fontKey << " size: " << fontSize << std::endl;
    }

    // �����ı�����
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface)
    {
        std::cerr << "�޷������ı�����: " << TTF_GetError() << std::endl;
        return;
    }

    // ��������
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_FreeSurface(surface);
        std::cerr << "�޷���������: " << SDL_GetError() << std::endl;
        return;
    }

    // ��Ⱦ�ı�
    SDL_Rect destRect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);

    // ������Դ
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void GameAPP::DrawText(SDL_Renderer* renderer,
    const std::string& text,
    const Vector& position,
    const SDL_Color& color,
    const std::string& fontKey,
    int fontSize)
{
    DrawText(renderer, text, static_cast<int>(position.x), static_cast<int>(position.y),
        color, fontKey, fontSize);
}

Vector GameAPP::GetTextSize(const std::string& text,
    const std::string& fontKey,
    int fontSize)
{
    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font)
    {
        return Vector::zero();
    }

    int width = 0, height = 0;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    return Vector(static_cast<float>(width), static_cast<float>(height));
}

SDL_Texture* GameAPP::CreateTextTexture(SDL_Renderer* renderer,
    const std::string& text,
    const SDL_Color& color,
    const std::string& fontKey,
    int fontSize)
{
    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font) return nullptr;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    return texture;
}

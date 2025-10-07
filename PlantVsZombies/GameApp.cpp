#include "GameApp.h"
#include "./UI/InputHandler.h"
#include "ResourceManager.h"
#include "./Game/Board.h"
#include <iostream>
#include <sstream>

GameAPP::GameAPP()
    : mBoard(nullptr)
    , mInputHandler(nullptr)
{
}

GameAPP::~GameAPP()
{
    
}

GameAPP& GameAPP::GetInstance()
{
    static GameAPP instance;
    return instance;
}

bool GameAPP::Initialize()
{
    mInputHandler = std::make_unique<InputHandler>();
    return true;
}

void GameAPP::CreateNewBoard()
{
    mBoard = std::make_unique<Board>();
}

void GameAPP::CloseGame()
{
    if (mInputHandler)
    {
        mInputHandler.reset();
    }
    CleanupResources();
}

void GameAPP::ClearTextCache()
{
    for (auto& cache : mTextCache)
    {
        if (cache.texture)
        {
            SDL_DestroyTexture(cache.texture);
        }
    }
    mTextCache.clear();
    std::cout << "清除文本缓存" << std::endl;
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

    // 使用资源管理器获取字体
    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font)
    {
        std::cerr << "无法获取字体: " << fontKey << " size: " << fontSize << std::endl;
    }

    // 创建文本表面
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface)
    {
        std::cerr << "无法创建文本表面: " << TTF_GetError() << std::endl;
        return;
    }

    // 创建纹理
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_FreeSurface(surface);
        std::cerr << "无法创建纹理: " << SDL_GetError() << std::endl;
        return;
    }

    // 渲染文本
    SDL_Rect destRect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);

    // 清理资源
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
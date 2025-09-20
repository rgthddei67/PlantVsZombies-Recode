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
        std::cerr << "渲染器无效，无法加载资源" << std::endl;
        return false;
    }

    // 先清理现有纹理
    ClearTextures();

    for (const auto& path : imagePaths)
    {
        // 使用SDL_image加载纹理
        SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
        if (!texture)
        {
            std::string errorMsg = "加载纹理失败: " + path + " - " + IMG_GetError();
            std::cerr << errorMsg << std::endl;

            // 清理已加载的纹理
            ClearTextures();
            return false;
        }

        gameTextures.push_back(texture);
        std::cout << "成功加载纹理: " << path << std::endl;
    }

    std::cout << "资源加载完成! 共加载 " << gameTextures.size()
        << "/" << imagePaths.size() << " 个纹理" << std::endl;
    return true;
}

void GameAPP::CloseGame(SDL_Renderer* renderer, SDL_Window* window)
{
    // 清理纹理资源
    ClearTextures();

    // 清理输入处理器
    if (g_pInputHandler != nullptr)
    {
        InputHandler::ReleaseInstance();
    }

    // 只清理应用相关的资源
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
    // 加载字体
    TTF_Font* font = TTF_OpenFont(fontName.c_str(), fontSize);
    if (!font)
    {
        std::cerr << "无法加载字体: " << TTF_GetError() << std::endl;
        return;
    }

    // 创建文本表面
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface)
    {
        TTF_CloseFont(font);
        std::cerr << "无法创建文本表面: " << TTF_GetError() << std::endl;
        return;
    }

    // 创建纹理
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_FreeSurface(surface);
        TTF_CloseFont(font);
        std::cerr << "无法创建纹理: " << SDL_GetError() << std::endl;
        return;
    }

    // 渲染文本
    SDL_Rect destRect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, nullptr, &destRect);

    // 清理资源
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
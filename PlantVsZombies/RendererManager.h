#pragma once
#ifndef _RENDERER_MANAGER_H
#define _RENDERER_MANAGER_H

#include <SDL2/SDL.h>
#include <memory>
#include <iostream>

class RendererManager {
private:
    SDL_Renderer* m_renderer = nullptr;

    RendererManager() = default;
    ~RendererManager() {
        Cleanup();
    }

    void Cleanup() {
        m_renderer = nullptr;
    }

public:
    RendererManager(const RendererManager&) = delete;
    RendererManager& operator=(const RendererManager&) = delete;

    static RendererManager& GetInstance() {
        static RendererManager instance;
        return instance;
	}

    void SetRenderer(SDL_Renderer* renderer) {
        m_renderer = renderer;
    }

    SDL_Renderer* GetRenderer() const {
        return m_renderer;
    }
};

#endif
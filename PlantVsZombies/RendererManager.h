#pragma once
#ifndef _RENDERER_MANAGER_H
#define _RENDERER_MANAGER_H

#include <SDL.h>
#include <memory>
#include <iostream>

struct SDLRendererDeleter {
    void operator()(SDL_Renderer* renderer) const {
        if (renderer) {
            //std::cout << "正在销毁SDL渲染器..." << std::endl;
            SDL_DestroyRenderer(renderer);
            //std::cout << "SDL渲染器已安全销毁" << std::endl;
        }
    }
};

class RendererManager {
private:
    std::unique_ptr<SDL_Renderer, SDLRendererDeleter> m_renderer;
	RendererManager() = default; // 私有构造函数
    ~RendererManager() {
        Cleanup();
    }
    void Cleanup() {
        if (m_renderer) {
            m_renderer.reset();
        }
    }
public:
    RendererManager(const RendererManager&) = delete;
    RendererManager& operator=(const RendererManager&) = delete;

    static RendererManager& GetInstance() {
        static RendererManager instance;
        return instance;
    }

    // 设置渲染器（转移所有权）
    void SetRenderer(SDL_Renderer* renderer) {
        if (!renderer) {
            std::cout << "错误: 尝试设置nullptr渲染器!" << std::endl;
            return;
        }
#ifdef _DEBUG
        std::cout << "设置渲染器: " << (void*)renderer << std::endl;
#endif
        m_renderer.reset(renderer); // 自动释放之前的渲染器（如果有）
    }

    // 获取渲染器（不转移所有权）
    SDL_Renderer* GetRenderer() const {
        if (!m_renderer) {
			std::cout << "RendererManager::GetRenderer:警告: 渲染器未初始化!" 
                << std::endl;
        }
        return m_renderer.get();
    }

    // 检查渲染器是否有效
    bool IsNullPtr() const {
        return m_renderer != nullptr;
    }

    // 获取渲染器信息
    void PrintRendererInfo() const {
        if (!m_renderer) {
            std::cout << "渲染器: 未初始化" << std::endl;
            return;
        }

        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(m_renderer.get(), &info) == 0) {
            std::cout << "渲染器信息:" << std::endl;
            std::cout << "  - 名称: " << info.name << std::endl;
            std::cout << "  - 标志: " << info.flags << std::endl;
            std::cout << "  - 纹理格式数量: " << info.num_texture_formats << std::endl;
            std::cout << "  - 最大纹理尺寸: " << info.max_texture_width << "x" << info.max_texture_height << std::endl;
        }
        else {
            std::cerr << "获取渲染器信息失败: " << SDL_GetError() << std::endl;
        }
    }
};

#endif
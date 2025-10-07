#pragma once
#ifndef _RENDERER_MANAGER_H
#define _RENDERER_MANAGER_H

#include <SDL.h>
#include <memory>
#include <iostream>

struct SDLRendererDeleter {
    void operator()(SDL_Renderer* renderer) const {
        if (renderer) {
            //std::cout << "��������SDL��Ⱦ��..." << std::endl;
            SDL_DestroyRenderer(renderer);
            //std::cout << "SDL��Ⱦ���Ѱ�ȫ����" << std::endl;
        }
    }
};

class RendererManager {
private:
    std::unique_ptr<SDL_Renderer, SDLRendererDeleter> m_renderer;
	RendererManager() = default; // ˽�й��캯��
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

    // ������Ⱦ����ת������Ȩ��
    void SetRenderer(SDL_Renderer* renderer) {
        if (!renderer) {
            std::cout << "����: ��������nullptr��Ⱦ��!" << std::endl;
            return;
        }
#ifdef _DEBUG
        std::cout << "������Ⱦ��: " << (void*)renderer << std::endl;
#endif
        m_renderer.reset(renderer); // �Զ��ͷ�֮ǰ����Ⱦ��������У�
    }

    // ��ȡ��Ⱦ������ת������Ȩ��
    SDL_Renderer* GetRenderer() const {
        if (!m_renderer) {
			std::cout << "RendererManager::GetRenderer:����: ��Ⱦ��δ��ʼ��!" 
                << std::endl;
        }
        return m_renderer.get();
    }

    // �����Ⱦ���Ƿ���Ч
    bool IsNullPtr() const {
        return m_renderer != nullptr;
    }

    // ��ȡ��Ⱦ����Ϣ
    void PrintRendererInfo() const {
        if (!m_renderer) {
            std::cout << "��Ⱦ��: δ��ʼ��" << std::endl;
            return;
        }

        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(m_renderer.get(), &info) == 0) {
            std::cout << "��Ⱦ����Ϣ:" << std::endl;
            std::cout << "  - ����: " << info.name << std::endl;
            std::cout << "  - ��־: " << info.flags << std::endl;
            std::cout << "  - �����ʽ����: " << info.num_texture_formats << std::endl;
            std::cout << "  - �������ߴ�: " << info.max_texture_width << "x" << info.max_texture_height << std::endl;
        }
        else {
            std::cerr << "��ȡ��Ⱦ����Ϣʧ��: " << SDL_GetError() << std::endl;
        }
    }
};

#endif
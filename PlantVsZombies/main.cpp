#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./GameAPP.h"
#include "GameMonitor.h"
#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanRenderer.h"
#include "Renderer/VulkanPipeline.h"
#include "Renderer/VulkanTexturePool.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

// Phase 2b 测试图：64×64 RGBA8 棋盘格，便于肉眼检验纹理采样和颜色没问题
static std::vector<uint8_t> MakeCheckerboardRGBA(int size, int cellPx)
{
    std::vector<uint8_t> px(size_t(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool dark = ((x / cellPx) ^ (y / cellPx)) & 1;
            uint8_t* p = &px[size_t(y) * size * 4 + size_t(x) * 4];
            if (dark) { p[0] = 240; p[1] = 80;  p[2] = 80;  p[3] = 255; } // 红
            else      { p[0] = 80;  p[1] = 240; p[2] = 240; p[3] = 255; } // 青
        }
    }
    return px;
}

// Phase 2b 烟雾测试：clear color 动画 + 测试三角形 + 一个 bindless 贴图 quad。
// 使用方式：PlantsVsZombies.exe -Vulkan
static int RunVulkanSmokeTest()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "[VulkanSmoke] SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow(
        "PVZ Vulkan Smoke Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1100, 600,
        SDL_WINDOW_VULKAN);
    if (!win) {
        std::cerr << "[VulkanSmoke] SDL_CreateWindow(VULKAN) failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

#ifdef _DEBUG
    const bool enableValidation = true;
#else
    const bool enableValidation = false;
#endif

    int rc = 0;
    {
        pvz::VulkanContext     ctx;
        pvz::VulkanRenderer    renderer;
        pvz::VulkanTexturePool texPool;
        pvz::VulkanPipeline    trianglePipe;
        pvz::VulkanPipeline    quadPipe;

        auto setup = [&]() -> bool {
            if (!ctx.Initialize(win, enableValidation)) {
                std::cerr << "[VulkanSmoke] VulkanContext::Initialize failed" << std::endl;
                rc = 2; return false;
            }
            if (!renderer.Initialize(&ctx)) {
                std::cerr << "[VulkanSmoke] VulkanRenderer::Initialize failed" << std::endl;
                rc = 3; return false;
            }

            pvz::VulkanPipeline::Desc triDesc{};
            triDesc.vertSpvPath = "Shader/spv/triangle.vert.spv";
            triDesc.fragSpvPath = "Shader/spv/triangle.frag.spv";
            triDesc.colorFormat = ctx.SwapchainFormat();
            if (!trianglePipe.Initialize(&ctx, triDesc)) {
                std::cerr << "[VulkanSmoke] triangle pipeline init failed "
                             "(did glslc pre-build step run? .spv next to exe?)"
                          << std::endl;
                rc = 5; return false;
            }

            if (!texPool.Initialize(&ctx)) {
                std::cerr << "[VulkanSmoke] VulkanTexturePool init failed" << std::endl;
                rc = 6; return false;
            }

            const int CHECKER_SIZE = 64;
            auto pixels = MakeCheckerboardRGBA(CHECKER_SIZE, 8);
            pvz::VulkanTexture* checker = texPool.CreateTextureRGBA8(
                CHECKER_SIZE, CHECKER_SIZE, pixels.data());
            if (!checker) {
                std::cerr << "[VulkanSmoke] CreateTextureRGBA8 failed" << std::endl;
                rc = 7; return false;
            }

            pvz::VulkanPipeline::Desc qd{};
            qd.vertSpvPath         = "Shader/spv/quad.vert.spv";
            qd.fragSpvPath         = "Shader/spv/quad.frag.spv";
            qd.colorFormat         = ctx.SwapchainFormat();
            qd.descriptorSetLayout = texPool.DescriptorSetLayout();
            qd.pushConstantSize    = 20;  // vec4 + uint
            qd.pushConstantStages  = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            qd.alphaBlend          = true;
            if (!quadPipe.Initialize(&ctx, qd)) {
                std::cerr << "[VulkanSmoke] quad pipeline init failed" << std::endl;
                rc = 8; return false;
            }

            renderer.SetTrianglePipeline(&trianglePipe);
            renderer.SetTexturedQuad(&quadPipe, texPool.DescriptorSet(),
                                     /*x=*/-0.9f, /*y=*/-0.9f,
                                     /*w=*/ 0.6f, /*h=*/ 0.6f,
                                     checker->bindlessIndex);
            return true;
        };

        if (setup()) {
            std::cout << "[VulkanSmoke] Render loop running. Close window or press ESC to quit." << std::endl;

            const Uint64 startMs = SDL_GetTicks64();
            bool running = true;
            uint32_t frameCount = 0;

            while (running) {
                SDL_Event ev;
                while (SDL_PollEvent(&ev)) {
                    if (ev.type == SDL_QUIT) running = false;
                    else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                }

                const float t = (SDL_GetTicks64() - startMs) / 1000.0f;
                const float r = 0.5f + 0.5f * std::sin(t * 1.0f);
                const float g = 0.5f + 0.5f * std::sin(t * 1.3f + 2.0f);
                const float b = 0.5f + 0.5f * std::sin(t * 1.7f + 4.0f);

                if (!renderer.DrawFrame(r, g, b, 1.0f)) {
                    running = false;
                    rc = 4;
                }
                ++frameCount;
            }

            const Uint64 elapsedMs = SDL_GetTicks64() - startMs;
            const double avgMs = elapsedMs / (double)std::max<uint32_t>(frameCount, 1);
            std::cout << "[VulkanSmoke] " << frameCount << " frames in "
                      << elapsedMs << " ms (" << avgMs << " ms/frame, "
                      << (1000.0 / avgMs) << " fps)" << std::endl;
        }

        // 析构顺序很关键：quadPipe / trianglePipe 持有的 layout 依赖于 texPool 的 set layout，
        // 但因为 layout 是 VulkanPipeline 内部新建的，所以它们各自独立销毁就行。
        // 析构走声明顺序的逆序：quadPipe → trianglePipe → texPool → renderer → ctx，正确。
    }

    SDL_DestroyWindow(win);
    SDL_Quit();
    return rc;
}

int main(int argc, char** argv)
{
    system("chcp 65001");
    system("cls");
    CrashHandler::Initialize();
    GameRandom::RandomizeSeed();

    GameMonitor::Init();

    // 检查命令行参数
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "-Debug" || arg == "-debug")
        {
            GameAPP::mDebugMode = true;
            GameAPP::mShowColliders = true;
            std::cout << "Debug模式已启用" << std::endl;
        }
        else if (arg == "-Vulkan" || arg == "-vulkan")
        {
            // 走 Vulkan 烟雾测试分支，不启动游戏
            int rc = RunVulkanSmokeTest();
            CrashHandler::Cleanup();
            return rc;
        }
    }

    int result = GameAPP::GetInstance().Run();

    CrashHandler::Cleanup();

    return result;
}
#include "GameAPP.h"
#include "Button.h"
#include "Slider.h"
#include "UIManager.h"
#include "InputHandler.h"
#include "ResourceManager.h"
#include "AudioSystem.h"
#include "Reanimator.h"
#include "ParticleSystem.h"
#include <iostream>
#include <sstream>

namespace UIFunctions
{
    static void ButtonClick()
    {
        std::cout << "点击" << std::endl;
        AudioSystem::PlaySound(AudioConstants::SOUND_BUTTONCLICK, 0.4f);
    }
    static void SliderChanged(float value)
    {
        std::cout << "滑动条值改变: " << value << std::endl;
    }

}

int SDL_main(int argc, char* argv[])
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) 
    {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 初始化SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    int initializedFlags = IMG_Init(imgFlags);

    if ((initializedFlags & imgFlags) != imgFlags) {
        std::cerr << "SDL_image初始化失败，请求: " << imgFlags
            << "，实际: " << initializedFlags << " - " << IMG_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // 初始化SDL_ttf
    if (TTF_Init() == -1) 
    {
        std::cerr << "SDL_ttf初始化失败: " << TTF_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return -1;
    }

    if (!AudioSystem::Initialize())
    {
        std::cerr << "警告: 音频初始化失败，游戏将继续运行但没有声音" << std::endl;
    }

    InputHandler* input = InputHandler::GetInstance();
    UIManager uiManager;
    GameAPP gameApp;

    // 设置默认字体路径
    Button::SetDefaultFontPath("./font/fzcq.ttf");

    // 创建窗口和渲染器
    SDL_Window* window = SDL_CreateWindow("Plant vs Zombies",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN);

    if (!window) {
        std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
        AudioSystem::Shutdown();
        Mix_Quit();
        gameApp.CloseGame();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) 
    {
        std::cerr << "渲染器创建失败: " << SDL_GetError() << std::endl;
        AudioSystem::Shutdown();
        Mix_Quit();
        gameApp.CloseGame();
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }

    // 设置渲染器颜色（蓝色背景）
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    ResourceManager& resourceManager = ResourceManager::GetInstance();
    resourceManager.Initialize(renderer);

    // 统一加载所有资源
    bool resourcesLoaded = true;
    resourcesLoaded &= resourceManager.LoadAllGameImages();
    resourcesLoaded &= resourceManager.LoadAllParticleTextures();
    resourcesLoaded &= resourceManager.LoadAllFonts();
    resourcesLoaded &= resourceManager.LoadAllSounds();
    resourcesLoaded &= resourceManager.LoadAllMusic();
	resourcesLoaded &= resourceManager.LoadAllAnimations();

    if (!resourcesLoaded)
    {
        AudioSystem::Shutdown();    
        Mix_Quit();                
        GameAPP::CleanupResources();
        ResourceManager::ReleaseInstance();
        gameApp.CloseGame();
		SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }
	ReanimationHolder animHolder(renderer); // 创建动画管理器
    ParticleSystem particleSystem(renderer); // 创建粒子特效系统管理器
    /*
	Reanimation* anim = 
        animHolder.AllocReanimation(250, 250, "./resources/reanim/Sun", 0.8f);
    if (anim) 
    {
        anim->SetLoopType(ReanimLoopType::REANIM_LOOP);
        anim->SetRate(8.0f);
    }
    */
    Reanimation* anim = 
		animHolder.AllocReanimation(400, 300, AnimationType::ANIM_SUN, 0.8f);
    // 创建按钮
    auto button1 = uiManager.CreateButton(Vector(100, 150));
    button1->SetAsCheckbox(true);
    button1->SetImageIndexes(1, 1, 1, 2);
    button1->SetClickCallBack(UIFunctions::ButtonClick);

    auto button2 = uiManager.CreateButton(Vector(300, 150), Vector(110, 25));
    button2->SetAsCheckbox(false);
    button2->SetImageIndexes(3, 4, 4, -1);
    button2->SetTextColor({ 255, 255, 255, 255 }); // 白色
    button2->SetHoverTextColor({ 0, 0, 0, 255 });  // 黑色
    button2->SetText(u8"W就W是W");
    button2->SetClickCallBack(UIFunctions::ButtonClick);

    auto slider = uiManager.CreateSlider(Vector(500, 150), Vector(135, 10), 0.0f, 100.0f, 0.0f);
    slider->SetChangeCallBack(UIFunctions::SliderChanged);
    bool running = true;
    SDL_Event event;

    SDL_Texture* particleTex = resourceManager.GetTexture("particle_1");
    if (!particleTex) {
        std::cerr << "错误: 粒子纹理加载失败！" << std::endl;
        resourcesLoaded = false;
    }
    else {
        std::cout << "粒子纹理加载成功" << std::endl;
    }

    while (running) 
    {
        // 处理事件
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT) 
            {
                running = false;
            }
            uiManager.ProcessMouseEvent(&event, input);
            input->ProcessEvent(&event);
        }

        input->Update();

        if (input->IsKeyDown(SDLK_ESCAPE)) 
        {
            running = false;
            break;
        }
        if (input->IsKeyDown(SDLK_SPACE))
        {
            particleSystem.EmitEffect
                (ParticleEffect::ZOMBIE_HEAD_OFF, 150, 100, 1);
        }
        
		// 更新板块
        uiManager.UpdateAll(input);
		animHolder.UpdateAll();
        particleSystem.UpdateAll();
		
        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);

        // 绘制板块
        uiManager.DrawAll(renderer);
        animHolder.DrawAll();
        particleSystem.DrawAll();

        // 更新屏幕
        SDL_RenderPresent(renderer);

        uiManager.ResetAllFrameStates();
    }
    AudioSystem::Shutdown();
    if (window != nullptr)
    {
        SDL_StopTextInput();
    }
    GameAPP::CleanupResources();
    ResourceManager::ReleaseInstance();
    gameApp.CloseGame();
    if (renderer != nullptr) 
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    Mix_Quit();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    //_CrtDumpMemoryLeaks();
    return 0;
}
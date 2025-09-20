#include "GameAPP.h"
#include "Button.h"
#include "ButtonManager.h"
#include "InputHandler.h"
#include <iostream>
#include <sstream>


void ButtonClick()
{
    std::cout << "点击" << std::endl;
}

int SDL_main(int argc, char* argv[])
{
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) 
    {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 初始化SDL_image
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) 
    {
        std::cerr << "SDL_image初始化失败: " << IMG_GetError() << std::endl;
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

    InputHandler* input = InputHandler::GetInstance();
    ButtonManager buttonManager;
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

    if (!gameApp.LoadGameResources(renderer)) 
    {
        gameApp.CloseGame(renderer, window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }

    // 创建按钮
    auto button1 = buttonManager.CreateButton(Vector(100, 150));
    button1->SetAsCheckbox(true);
    button1->SetImageIndexes(1, 1, 1, 2);
    button1->SetOnClick(ButtonClick);

    auto button2 = buttonManager.CreateButton(Vector(300, 150), Vector(110, 25));
    button2->SetAsCheckbox(false);
    button2->SetImageIndexes(3, 4, 4, -1);
    button2->SetTextColor({ 255, 255, 255, 255 }); // 白色
    button2->SetHoverTextColor({ 0, 0, 0, 255 });  // 黑色
    button2->SetText(u8"一e");
    button2->SetOnClick(ButtonClick);

    bool running = true;
    SDL_Event event;

    while (running) 
    {
        // 处理事件
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT) 
            {
                running = false;
            }
            buttonManager.ProcessMouseEvent(&event);
            input->ProcessEvent(&event);
        }

        input->Update();

        if (input->IsKeyDown(SDLK_ESCAPE)) 
        {
            running = false;
            break;
        }

        buttonManager.UpdateAll(input);

        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);

        // 绘制按钮
        buttonManager.DrawAll(renderer, gameApp.GetGameTextures());

        // 更新屏幕
        SDL_RenderPresent(renderer);

        buttonManager.ResetAllFrameStates();
        SDL_Delay(16);  // 约60FPS
    }

    gameApp.CloseGame(renderer, window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
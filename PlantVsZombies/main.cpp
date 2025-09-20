#include "GameAPP.h"
#include "Button.h"
#include "ButtonManager.h"
#include "InputHandler.h"
#include <iostream>
#include <sstream>


void ButtonClick()
{
    std::cout << "���" << std::endl;
}

int SDL_main(int argc, char* argv[])
{
    // ��ʼ��SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) 
    {
        std::cerr << "SDL��ʼ��ʧ��: " << SDL_GetError() << std::endl;
        return -1;
    }

    // ��ʼ��SDL_image
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) 
    {
        std::cerr << "SDL_image��ʼ��ʧ��: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // ��ʼ��SDL_ttf
    if (TTF_Init() == -1) 
    {
        std::cerr << "SDL_ttf��ʼ��ʧ��: " << TTF_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return -1;
    }

    InputHandler* input = InputHandler::GetInstance();
    ButtonManager buttonManager;
    GameAPP gameApp;

    // ����Ĭ������·��
    Button::SetDefaultFontPath("./font/fzcq.ttf");

    // �������ں���Ⱦ��
    SDL_Window* window = SDL_CreateWindow("Plant vs Zombies",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN);

    if (!window) {
        std::cerr << "���ڴ���ʧ��: " << SDL_GetError() << std::endl;
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
        std::cerr << "��Ⱦ������ʧ��: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }

    // ������Ⱦ����ɫ����ɫ������
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

    // ������ť
    auto button1 = buttonManager.CreateButton(Vector(100, 150));
    button1->SetAsCheckbox(true);
    button1->SetImageIndexes(1, 1, 1, 2);
    button1->SetOnClick(ButtonClick);

    auto button2 = buttonManager.CreateButton(Vector(300, 150), Vector(110, 25));
    button2->SetAsCheckbox(false);
    button2->SetImageIndexes(3, 4, 4, -1);
    button2->SetTextColor({ 255, 255, 255, 255 }); // ��ɫ
    button2->SetHoverTextColor({ 0, 0, 0, 255 });  // ��ɫ
    button2->SetText(u8"һe");
    button2->SetOnClick(ButtonClick);

    bool running = true;
    SDL_Event event;

    while (running) 
    {
        // �����¼�
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

        // ����
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);

        // ���ư�ť
        buttonManager.DrawAll(renderer, gameApp.GetGameTextures());

        // ������Ļ
        SDL_RenderPresent(renderer);

        buttonManager.ResetAllFrameStates();
        SDL_Delay(16);  // Լ60FPS
    }

    gameApp.CloseGame(renderer, window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
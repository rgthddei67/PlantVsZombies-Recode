#include "GameAPP.h"
#include "Button.h"
#include "Slider.h"
#include "UIManager.h"
#include "InputHandler.h"
#include "ResourceManager.h"
#include "AudioSystem.h"
#include <iostream>
#include <sstream>


void ButtonClick()
{
    std::cout << "���" << std::endl;
    AudioSystem::PlaySound(AudioConstants::SOUND_BUTTONCLICK);
}

void SliderChanged(float value)
{
    std::cout << "������ֵ�ı�: " << value << std::endl;
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

    Mix_AllocateChannels(16); // ����16����ƵƵ��

    if (!AudioSystem::Initialize())
    {
        std::cerr << "����: ��Ƶ��ʼ��ʧ�ܣ���Ϸ���������е�û������" << std::endl;
    }

    InputHandler* input = InputHandler::GetInstance();
    UIManager uiManager;
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
        AudioSystem::Shutdown();
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
        std::cerr << "��Ⱦ������ʧ��: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        AudioSystem::Shutdown();
        gameApp.CloseGame();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        Mix_Quit();
        return -1;
    }

    // ������Ⱦ����ɫ����ɫ������
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    ResourceManager& resourceManager = ResourceManager::GetInstance();
    resourceManager.Initialize(renderer);

    // ͳһ����������Դ
    bool resourcesLoaded = true;
    resourcesLoaded &= resourceManager.LoadAllGameImages();
    resourcesLoaded &= resourceManager.LoadAllParticleTextures();
    resourcesLoaded &= resourceManager.LoadAllFonts();
    resourcesLoaded &= resourceManager.LoadAllSounds();
    resourcesLoaded &= resourceManager.LoadAllMusic();

    if (!resourcesLoaded)
    {
        AudioSystem::Shutdown();
        GameAPP::CleanupResources();
        ResourceManager::ReleaseInstance();
        gameApp.CloseGame();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }

    // ������ť
    auto button1 = uiManager.CreateButton(Vector(100, 150));
    button1->SetAsCheckbox(true);
    button1->SetImageIndexes(1, 1, 1, 2);
    button1->SetClickCallBack(ButtonClick);

    auto button2 = uiManager.CreateButton(Vector(300, 150), Vector(110, 25));
    button2->SetAsCheckbox(false);
    button2->SetImageIndexes(3, 4, 4, -1);
    button2->SetTextColor({ 255, 255, 255, 255 }); // ��ɫ
    button2->SetHoverTextColor({ 0, 0, 0, 255 });  // ��ɫ
    button2->SetText(u8"һe");
    button2->SetClickCallBack(ButtonClick);

    auto slider = uiManager.CreateSlider(Vector(500, 150), Vector(135, 10), 0.0f, 100.0f, 0.0f);
    slider->SetChangeCallBack(SliderChanged);
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
            uiManager.ProcessMouseEvent(&event, input);
            input->ProcessEvent(&event);
        }

        input->Update();

        if (input->IsKeyDown(SDLK_ESCAPE)) 
        {
            running = false;
            break;
        }

        uiManager.UpdateAll(input);

        // ����
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);

        // ����ȫ��UI
        uiManager.DrawAll(renderer);

        // ������Ļ
        SDL_RenderPresent(renderer);

        uiManager.ResetAllFrameStates();
        SDL_Delay(16);  // Լ60FPS
    }
    AudioSystem::Shutdown();
    GameAPP::CleanupResources();
    ResourceManager::ReleaseInstance();
    gameApp.CloseGame();
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
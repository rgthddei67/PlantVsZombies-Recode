#define SDL_MAIN_HANDLED
#include "./GameAPP.h"
#include "./UI/Button.h"
#include "./UI/Slider.h"
#include "./UI/UIManager.h"
#include "./UI/InputHandler.h"
#include "./ResourceManager.h"
#include "./RendererManager.h"
#include "./Game/AudioSystem.h"
#include "./Reanimation/Reanimator.h"
#include "./ParticleSystem/ParticleSystem.h"
#include "./Game/GameObject.h"
#include "./Game/GameObjectManager.h"
#include "./Game/Component.h"
#include "./Game/CollisionSystem.h"
#include "./Game/ClickableComponent.h"
#include "./TestObject.h"
#include "./Game/AnimatedObject.h"
#include "./Game/Sun.h"
#include "./Game/Board.h"
#include <iostream>
#include <sstream>

// ȫ������ϵͳ
extern std::unique_ptr<ParticleSystem> g_particleSystem;  
std::unique_ptr<ParticleSystem> g_particleSystem = nullptr;

namespace UIFunctions
{
    static void ImageButtonClick()
    {
        if (g_particleSystem != nullptr)
        {
            g_particleSystem->EmitEffect(
				ParticleEffect::ZOMBIE_HEAD_OFF, 100, 150, 1); 
        }
        AudioSystem::PlaySound(AudioConstants::SOUND_BUTTONCLICK, 0.4f);
    }
    static void ClickedButtonClick()
    {
        if (g_particleSystem != nullptr)
        {
            g_particleSystem->EmitEffect(
                ParticleEffect::PEA_BULLET_HIT, 100, 150, 5);
        }
        AudioSystem::PlaySound(AudioConstants::SOUND_BUTTONCLICK, 0.4f);
    }
    static void SliderChanged(float value)
    {
        std::cout << "������ֵ�ı�: " << value << std::endl;
    }

}

void CreateAnimationTest() {
    // ����̫������
    auto sun = GameObjectManager::GetInstance().CreateGameObject<Sun>(
        Vector(400, 300)           // λ��
    );
}

int main(int argc, char* argv[])
{
    // ��ʼ��SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) 
    {
        std::cerr << "SDL��ʼ��ʧ��: " << SDL_GetError() << std::endl;
        return -1;
    }

    // ��ʼ��SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    int initializedFlags = IMG_Init(imgFlags);

    if ((initializedFlags & imgFlags) != imgFlags) {
        std::cerr << "SDL_image��ʼ��ʧ�ܣ�����: " << imgFlags
            << "��ʵ��: " << initializedFlags << " - " << IMG_GetError() << std::endl;
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

    if (!AudioSystem::Initialize())
    {
        std::cerr << "����: ��Ƶ��ʼ��ʧ�ܣ���Ϸ���������е�û������" << std::endl;
    }

    GameAPP::GetInstance().Initialize();
    UIManager uiManager;

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
        Mix_Quit();
        GameAPP::GetInstance().CloseGame();
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
        AudioSystem::Shutdown();
        Mix_Quit();
        GameAPP::GetInstance().CloseGame();
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
	
	RendererManager::GetInstance().SetRenderer(renderer);
    ResourceManager& resourceManager = ResourceManager::GetInstance();
    resourceManager.Initialize(renderer);

    // ͳһ����������Դ
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
        GameAPP::GetInstance().CleanupResources();
        ResourceManager::ReleaseInstance();
        GameAPP::GetInstance().CloseGame();
		SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -1;
    }
	g_particleSystem = std::make_unique<ParticleSystem>(renderer); // ��ʼ��ȫ������ϵͳ
	CreateAnimationTest(); 
    // ������ť
    auto button1 = uiManager.CreateButton(Vector(100, 150));
    button1->SetAsCheckbox(true);
    button1->SetImageIndexes(1, 1, 1, 2);
    button1->SetClickCallBack(UIFunctions::ClickedButtonClick);
    auto button2 = uiManager.CreateButton(Vector(300, 150), Vector(110, 25));
    button2->SetAsCheckbox(false);
    button2->SetImageIndexes(3, 4, 4, -1);
    button2->SetTextColor({ 255, 255, 255, 255 }); // ��ɫ
    button2->SetHoverTextColor({ 0, 0, 0, 255 });  // ��ɫ
    button2->SetText(u8"�ٻ���ͷ��Ч");
    button2->SetClickCallBack(UIFunctions::ImageButtonClick);
    auto slider = uiManager.CreateSlider(Vector(500, 150), Vector(135, 10), 0.0f, 100.0f, 0.0f);
    slider->SetChangeCallBack(UIFunctions::SliderChanged);
    bool running = true;
    SDL_Event event;
    while (running) 
    {
        auto& input = GameAPP::GetInstance().GetInputHandler();
        // �����¼�
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT) 
            {
                running = false;
            }
            uiManager.ProcessMouseEvent(&event, &input);
            input.ProcessEvent(&event);
        }
        if (input.IsKeyReleased(SDLK_ESCAPE))
        {
            running = false;
            break;
        }
		// ���°��
        uiManager.UpdateAll(&input);
		g_particleSystem->UpdateAll();
        GameObjectManager::GetInstance().Update();
        CollisionSystem::GetInstance().Update();
		
        // ����
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);

        // ���ư��
        uiManager.DrawAll(renderer);
        g_particleSystem->DrawAll();
        GameObjectManager::GetInstance().DrawAll(renderer);
        // ������Ļ
        SDL_RenderPresent(renderer);

        uiManager.ResetAllFrameStates();

        input.Update();
    }
    g_particleSystem.reset();
    GameObjectManager::GetInstance().ClearAll();
    CollisionSystem::GetInstance().ClearAll();
    GameAPP::GetInstance().CleanupResources();
    ResourceManager::ReleaseInstance();
    AudioSystem::Shutdown();

    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
        RendererManager::GetInstance().SetRenderer(nullptr);
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    GameAPP::GetInstance().CloseGame();
    Mix_Quit();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
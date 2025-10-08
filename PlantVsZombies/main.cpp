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

// 全局粒子系统
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
        std::cout << "滑动条值改变: " << value << std::endl;
    }

}

void CreateAnimationTest() {
    // 创建太阳动画
    auto sun = GameObjectManager::GetInstance().CreateGameObject<Sun>(
        Vector(400, 300)           // 位置
    );
}

int main(int argc, char* argv[])
{
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

    GameAPP::GetInstance().Initialize();
    UIManager uiManager;

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
        std::cerr << "渲染器创建失败: " << SDL_GetError() << std::endl;
        AudioSystem::Shutdown();
        Mix_Quit();
        GameAPP::GetInstance().CloseGame();
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
	
	RendererManager::GetInstance().SetRenderer(renderer);
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
	g_particleSystem = std::make_unique<ParticleSystem>(renderer); // 初始化全局粒子系统
	CreateAnimationTest(); 
    // 创建按钮
    auto button1 = uiManager.CreateButton(Vector(100, 150));
    button1->SetAsCheckbox(true);
    button1->SetImageIndexes(1, 1, 1, 2);
    button1->SetClickCallBack(UIFunctions::ClickedButtonClick);
    auto button2 = uiManager.CreateButton(Vector(300, 150), Vector(110, 25));
    button2->SetAsCheckbox(false);
    button2->SetImageIndexes(3, 4, 4, -1);
    button2->SetTextColor({ 255, 255, 255, 255 }); // 白色
    button2->SetHoverTextColor({ 0, 0, 0, 255 });  // 黑色
    button2->SetText(u8"召唤掉头特效");
    button2->SetClickCallBack(UIFunctions::ImageButtonClick);
    auto slider = uiManager.CreateSlider(Vector(500, 150), Vector(135, 10), 0.0f, 100.0f, 0.0f);
    slider->SetChangeCallBack(UIFunctions::SliderChanged);
    bool running = true;
    SDL_Event event;
    while (running) 
    {
        auto& input = GameAPP::GetInstance().GetInputHandler();
        // 处理事件
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
		// 更新板块
        uiManager.UpdateAll(&input);
		g_particleSystem->UpdateAll();
        GameObjectManager::GetInstance().Update();
        CollisionSystem::GetInstance().Update();
		
        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderClear(renderer);

        // 绘制板块
        uiManager.DrawAll(renderer);
        g_particleSystem->DrawAll();
        GameObjectManager::GetInstance().DrawAll(renderer);
        // 更新屏幕
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
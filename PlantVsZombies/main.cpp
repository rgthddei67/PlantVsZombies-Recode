#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./DeltaTime.h"
#include "./GameMonitor.h"
#include "./GameAPP.h"
#include "./UI/Button.h"
#include "./UI/Slider.h"
#include "./UI/UIManager.h"
#include "./UI/InputHandler.h"
#include "./ResourceManager.h"
#include "./Game/Plant/PlantDataManager.h"
#include "./RendererManager.h"
#include "./Game/AudioSystem.h"
#include "./Reanimation/Reanimation.h"
#include "./ParticleSystem/ParticleSystem.h"
#include "./Game/GameObject.h"
#include "./Game/GameObjectManager.h"
#include "./Game/Component.h"
#include "./Game/CollisionSystem.h"
#include "./Game/ClickableComponent.h"
#include "./Game/AnimatedObject.h"
#include "./Game/Sun.h"
#include "./Game/Board.h"
#include "./Game/SceneManager.h"
#include "./Game/GameScene.h"
#include <iostream>
#include <sstream>

// 全局粒子系统
extern std::unique_ptr<ParticleSystem> g_particleSystem;

int main(int argc, char* argv[])
{
	CrashHandler::Initialize();
	GameRandom::RandomizeSeed();
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
	auto& sceneManager = SceneManager::GetInstance();
	sceneManager.RegisterScene<GameScene>("GameScene");

	// 设置默认字体路径
	Button::SetDefaultFontPath("./font/fzcq.ttf");

	// 创建窗口和渲染器
	SDL_Window* window = SDL_CreateWindow("Plants Vs Zombies",
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

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // 像素完美渲染
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");		 // 启用垂直同步

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
		SDL_RENDERER_ACCELERATED |
		SDL_RENDERER_PRESENTVSYNC |
		SDL_RENDERER_TARGETTEXTURE);

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
	PlantDataManager& plantMgr = PlantDataManager::GetInstance();
	plantMgr.Initialize();
	ResourceManager& resourceManager = ResourceManager::GetInstance();

	if (!resourceManager.Initialize(renderer, "./resources/resources.xml")) {
		std::cerr << "ResourceManager 初始化失败！" << std::endl;
		AudioSystem::Shutdown();
		Mix_Quit();
		GameAPP::GetInstance().CloseGame();
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		TTF_Quit();
		IMG_Quit();
		SDL_Quit();
		return -1;
	}

	Button::SetDefaultFontPath("./font/fzcq.ttf");

	// 统一加载所有资源
	bool resourcesLoaded = true;
	resourcesLoaded &= resourceManager.LoadAllGameImages();
	resourcesLoaded &= resourceManager.LoadAllParticleTextures();
	resourcesLoaded &= resourceManager.LoadAllFonts();
	resourcesLoaded &= resourceManager.LoadAllSounds();
	resourcesLoaded &= resourceManager.LoadAllMusic();
	resourcesLoaded &= resourceManager.LoadAllReanimations();

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
	// 创建按钮
	/*
	auto button1 = uiManager.CreateButton(Vector(100, 150));
	button1->SetAsCheckbox(true);
	button1->SetImageKeys("IMAGE_options_checkbox0", "IMAGE_options_checkbox0",
		"IMAGE_options_checkbox0", "IMAGE_options_checkbox1");
	button1->SetClickCallBack(UIFunctions::ClickedButtonClick);
	auto button2 = uiManager.CreateButton(Vector(300, 150), Vector(110, 25));
	button2->SetAsCheckbox(false);
	button2->SetImageKeys("IMAGE_SeedChooser_Button2", "IMAGE_SeedChooser_Button2_Glow",
		"IMAGE_SeedChooser_Button2_Glow", "IMAGE_SeedChooser_Button2");
	button2->SetTextColor({ 255, 255, 255, 255 }); // 白色
	button2->SetHoverTextColor({ 0, 0, 0, 255 });  // 黑色
	button2->SetText(u8"召唤掉头特效");
	button2->SetClickCallBack(UIFunctions::ImageButtonClick);
	auto slider = uiManager.CreateSlider(Vector(500, 150), Vector(135, 10), 0.0f, 100.0f, 0.0f);
	slider->SetChangeCallBack(UIFunctions::SliderChanged);
	*/
	DeltaTime::Reset();
	sceneManager.SwitchTo("GameScene");
	bool running = true;
	SDL_Event event;
	while (running && !sceneManager.IsEmpty())
	{
		DeltaTime::BeginFrame();
		auto& input = GameAPP::GetInstance().GetInputHandler();
		// 处理事件
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				running = false;
			}
			input.ProcessEvent(&event);
			sceneManager.HandleEvent(event, input);
		}
		if (input.IsKeyReleased(SDLK_F3))
		{
			AudioSystem::PlaySound(AudioConstants::SOUND_BUTTONCLICK, 0.5f);
			GameAPP& app = GameAPP::GetInstance();
			app.mDebugMode = !app.mDebugMode;
			app.mShowColliders = !app.mShowColliders;
			g_particleSystem->EmitEffect(ParticleType::ZOMBIE_HEAD_OFF, input.GetMousePosition(), 5);
		}
		if (input.IsKeyReleased(SDLK_ESCAPE))
		{
			running = false;
			break;
		}
		// 更新板块
		sceneManager.Update();
		
		// 清屏
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // 黑色背景
		SDL_RenderClear(renderer);

		// 绘制板块
		sceneManager.Draw(renderer);

		// 更新屏幕
		SDL_RenderPresent(renderer);
#ifdef _DEBUG
		static int MousePoint = 0;
		if (MousePoint++ % 40 == 0)
		{
			std::cout << "Mouse Position: "
				<< input.GetMousePosition().x << ", "
				<< input.GetMousePosition().y << std::endl;
		}
#endif
		input.Update();
	}
	//GameMonitor::PrintComprehensiveReport();
	g_particleSystem.reset();
	GameObjectManager::GetInstance().ClearAll();
	CollisionSystem::GetInstance().ClearAll();
	ResourceManager::ReleaseInstance();
	AudioSystem::Shutdown();
	if (RendererManager::GetInstance().GetRenderer()) {
		SDL_DestroyRenderer(RendererManager::GetInstance().GetRenderer());
		RendererManager::GetInstance().SetRenderer(nullptr);
	}
	GameAPP::GetInstance().CloseGame();
	CrashHandler::Cleanup();
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
	return 0;
}
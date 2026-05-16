#include "./GameAPP.h"
#include "./UI/InputHandler.h"
#include "./ResourceManager.h"
#include "./Game/SceneManager.h"
#include "./Game/GameScene.h"
#include "./Game/MainMenuScene.h"
#include "./Game/PlantAlmanacScene.h"
#include "./Game/AlmanacScene.h"
#include "./Game/ZombieAlmanacScene.h"
#include "./CursorManager.h"
#include "./Game/AudioSystem.h"
#include "./DeltaTime.h"
#include "./ParticleSystem/ParticleSystem.h"
#include "./Game/GameObjectManager.h"
#include "./Game/CollisionSystem.h"
#include "./Game/Plant/GameDataManager.h"
#include "./Game/RenderOrder.h"

#include "./Game/Plant/PeaShooter.h"
#include "./Game/Plant/SunFlower.h"
#include "./Game/Plant/CherryBomb.h"
#include "./Game/Plant/WallNut.h"
#include "./Game/Plant/PotatoMine.h"
#include "./Game/Plant/SnowPeaShooter.h"
#include "./Game/Plant/Chomper.h"
#include "./Game/Plant/Repeater.h"

#include "./Game/Zombie/Zombie.h"
#include "./Game/Zombie/ConeZombie.h"
#include "./Game/Zombie/Polevaulter.h"
#include "./Game/Zombie/BucketZombie.h"
#include "./Game/Zombie/FastBucketZombie.h"

#include "./Game/Board.h"

#include <iostream>

GameAPP::GameAPP()
	: mInputHandler(nullptr)
	, mWindow(nullptr)
	, m_glContext(nullptr)
	, mRunning(false)
	, mInitialized(false)
{
	mHaveCards.reserve(64);
	mHaveCards.push_back(PlantType::PLANT_PEASHOOTER);
}

GameAPP::~GameAPP()
{
}

GameAPP& GameAPP::GetInstance()
{
	static GameAPP instance;
	return instance;
}

bool GameAPP::InitializeSDL()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
		return false;
	}
	return true;
}

bool GameAPP::InitializeSDL_Image()
{
	int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
	int initializedFlags = IMG_Init(imgFlags);

	if ((initializedFlags & imgFlags) != imgFlags) {
		std::cerr << "SDL_image初始化失败，请求: " << imgFlags
			<< "，实际: " << initializedFlags << " - " << IMG_GetError() << std::endl;
		return false;
	}
	return true;
}

bool GameAPP::InitializeSDL_TTF()
{
	if (TTF_Init() == -1)
	{
		std::cerr << "SDL_ttf初始化失败: " << TTF_GetError() << std::endl;
		return false;
	}
	return true;
}

bool GameAPP::InitializeAudioSystem()
{
	if (!AudioSystem::Initialize())
	{
		std::cerr << "警告: 音频初始化失败，游戏将继续运行但没有声音" << std::endl;
	}
	return true;
}

bool GameAPP::CreateWindowAndRenderer()
{
	// 设置 OpenGL 版本 
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// 创建带 OpenGL 标志的窗口
	mWindow = SDL_CreateWindow(u8"植物大战僵尸中文版",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		SCENE_WIDTH, SCENE_HEIGHT,
		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	if (!mWindow) {
		std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
		return false;
	}

	// 创建 OpenGL 上下文
	m_glContext = SDL_GL_CreateContext(mWindow);
	if (!m_glContext) {
		std::cerr << "OpenGL上下文创建失败: " << SDL_GetError() << std::endl;
		return false;
	}

	// 初始化 glad (如果使用 glad)
	if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
		std::cerr << "glad初始化失败" << std::endl;
		return false;
	}

	// 创建 Graphics 实例并初始化
	m_graphics = std::make_unique<Graphics>();
	if (!m_graphics->Initialize(SCENE_WIDTH, SCENE_HEIGHT)) {
		std::cerr << "Graphics 初始化失败" << std::endl;
		return false;
	}

	// 设置默认清屏颜色
	m_graphics->SetClearColor(255, 255, 255, 255);

	return true;
}

bool GameAPP::InitializeResourceManager()
{
	if (!CursorManager::GetInstance().Initialize()) {
		std::cerr << "光标管理器创建失败！" << std::endl;
		return false;
	}

	CursorManager::GetInstance().SetDefaultCursor();

	GameDataManager& plantMgr = GameDataManager::GetInstance();
	plantMgr.Initialize();

	ResourceManager& resourceManager = ResourceManager::GetInstance();

	if (!resourceManager.Initialize("./resources/resources.xml")) {
		std::cerr << "ResourceManager 初始化失败！" << std::endl;
		return false;
	}

	return true;
}

bool GameAPP::LoadAllResources()
{
	ResourceManager& resourceManager = ResourceManager::GetInstance();
	bool resourcesLoaded = true;

	resourcesLoaded &= resourceManager.LoadAllGameImages();
	resourcesLoaded &= resourceManager.LoadAllImagesFromPath();
	resourcesLoaded &= resourceManager.LoadAllParticleTextures();
	resourcesLoaded &= resourceManager.LoadAllFonts();
	resourcesLoaded &= resourceManager.LoadAllSounds();
	resourcesLoaded &= resourceManager.LoadAllMusic();
	resourcesLoaded &= resourceManager.LoadAllReanimations();

	if (!resourcesLoaded)
	{
		std::cerr << "资源加载失败！" << std::endl;
		return false;
	}

	// 所有 reanim 与其部件纹理就绪后，构建图集页（消除批渲染的 32 纹理单元抖动）
	resourceManager.BuildReanimAtlases();

	return true;
}

bool GameAPP::Initialize()
{
	if (mInitialized) return true;

	// 设置默认字体路径 
	Button::SetDefaultFontPath(ResourceKeys::Fonts::FONT_FZCQ);

	mInitialized = true;
	return true;
}

int GameAPP::Run()
{
	// 初始化各个系统
	if (!InitializeSDL()) return -1;
	if (!InitializeSDL_Image()) {
		SDL_Quit();
		return -2;
	}
	if (!InitializeSDL_TTF()) {
		IMG_Quit();
		SDL_Quit();
		return -3;
	}
	if (!InitializeAudioSystem()) {
		// 音频失败仍继续
	}

	// 初始化 GameAPP 自身
	if (!Initialize()) {
		CleanupResources();
		TTF_Quit();
		IMG_Quit();
		SDL_Quit();
		return -4;
	}

	// 创建窗口和渲染器 
	if (!CreateWindowAndRenderer()) {
		CleanupResources();
		AudioSystem::Shutdown();
		TTF_Quit();
		IMG_Quit();
		SDL_Quit();
		return -5;
	}

	// 初始化资源管理器
	if (!InitializeResourceManager()) {
		CleanupResources();
		AudioSystem::Shutdown();
		m_graphics.reset();
		if (m_glContext) SDL_GL_DeleteContext(m_glContext);
		SDL_DestroyWindow(mWindow);
		TTF_Quit();
		IMG_Quit();
		SDL_Quit();
		return -6;
	}

	// 加载所有资源
	if (!LoadAllResources()) {
		CleanupResources();
		AudioSystem::Shutdown();
		CursorManager::GetInstance().Cleanup();
		m_graphics.reset();
		if (m_glContext) SDL_GL_DeleteContext(m_glContext);
		SDL_DestroyWindow(mWindow);
		TTF_Quit();
		IMG_Quit();
		SDL_Quit();
		return -7;
	}

	if (!mGameInfoSaver.LoadPlayerInfo())
	{
		std::cout << "无法加载玩家存档数据！可能是没有存档!" << std::endl;
	}

	SDL_GL_SetSwapInterval(static_cast<int>(this->mVsync));

	mInputHandler = std::make_unique<InputHandler>(m_graphics.get());
	g_particleSystem = std::make_unique<ParticleSystem>(m_graphics.get());

	if (g_particleSystem) {
		g_particleSystem->LoadXMLConfigs("./resources/particles/config");
	}

	auto& sceneManager = SceneManager::GetInstance();

	sceneManager.RegisterScene<MainMenuScene>("MainMenuScene");
	sceneManager.RegisterScene<AlmanacScene>("AlmanacScene");
	sceneManager.RegisterScene<GameScene>("GameScene");
	sceneManager.RegisterScene<PlantAlmanacScene>("PlantAlmanacScene");
	sceneManager.RegisterScene<ZombieAlmanacScene>("ZombieAlmanacScene");

	sceneManager.SwitchTo("MainMenuScene");

	DeltaTime::Reset();

	mRunning = true;
	SDL_Event event;

	while (mRunning && !sceneManager.IsEmpty())
	{
		DeltaTime::BeginFrame();
		// 处理事件
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				mRunning = false;
			}
			mInputHandler->ProcessEvent(&event);
		}

		// 更新
		CursorManager::GetInstance().ResetHoverCount();
		sceneManager.Update();
		CursorManager::GetInstance().Update();

		// 渲染
		Draw();

#ifdef _DEBUG
		static int MousePoint = 0;
		if (MousePoint++ % 40 == 0)
		{
			Vector mousePos = mInputHandler->GetMouseWorldPosition();
			std::cout << "Mouse World Position: " << mousePos.x << "，" << mousePos.y << std::endl;
			std::cout << "Mouse Screen Position: " << mInputHandler->GetMousePosition().x << ", "
				<< mInputHandler->GetMousePosition().y << std::endl;
		}
#endif

		mInputHandler->Update();

		// Profiler::Get().EndFrame();
	}

	// 清理
	Shutdown();

	return 0;
}

void GameAPP::Draw()
{
	// 清除颜色缓冲
	m_graphics->Clear();

	// 处理多线程渲染命令
	m_graphics->ProcessCommandQueue();

	// 绘制场景
	SceneManager::GetInstance().Draw(m_graphics.get());

	// 提交批处理并执行绘制
	// PROFILE_SCOPE("8.FinalFlush");
	m_graphics->FlushBatch();

	// 交换缓冲区
	// PROFILE_SCOPE("9.SwapWindow(vsync)");
	SDL_GL_SwapWindow(mWindow);
}

void GameAPP::Shutdown()
{
	if (mRunning) return;

	if (!mGameInfoSaver.SavePlayerInfo()) {
		std::cout << "错误: 无法保存玩家数据！ 你的数据将会清空！" << std::endl;
	}

	// 清理粒子系统
	g_particleSystem.reset();

	SceneManager::GetInstance().ClearCurrentScene();

	// 清理游戏对象和碰撞系统
	GameObjectManager::GetInstance().ClearAll();
	CollisionSystem::GetInstance().ClearAll();

	// 清理资源管理器 (会释放 OpenGL 纹理)
	ResourceManager::ReleaseInstance();

	// 清理文字缓存 (Graphics 内部有缓存)
	if (m_graphics) {
		m_graphics->ClearTextCache();
	}

	// 清理音频系统
	AudioSystem::Shutdown();

	// 清理输入处理器
	mInputHandler.reset();

	// 清理 Graphics
	m_graphics.reset();

	// 清理 OpenGL 上下文
	if (m_glContext) {
		SDL_GL_DeleteContext(m_glContext);
		m_glContext = nullptr;
	}

	// 清理窗口
	if (mWindow) {
		SDL_DestroyWindow(mWindow);
		mWindow = nullptr;
	}

	// 清理光标管理器
	CursorManager::GetInstance().Cleanup();

	// 清理 SDL 子系统
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();

	mRunning = false;
	mInitialized = false;
}

void GameAPP::CleanupResources()
{
	ResourceManager::GetInstance().CleanupUnusedFontSizes();
}

void GameAPP::DrawText(const std::string& text, const Vector& position,
	const glm::vec4& color,
	const std::string& fontKey, int fontSize)
{
	if (!m_graphics) return;
	m_graphics->DrawText(text, fontKey, fontSize, color, position.x, position.y);
}

Background GameAPP::GetBackgroundID(int level) const
{
	if (level >= 1 && level <= 9) {
		return Background::GROUND_DAY;   // 白天
	}
	else if (level >= 10 && level <= 19) {
		return Background::GROUND_NIGHT; // 黑天
	}
	else {
		return Background::GROUND_DAY;   // 默认白天
	}
}

std::shared_ptr<Plant> GameAPP::InstantiatePlant(PlantType plantType, Board* board, int row, int column, bool isPreview)
{
	// TODO 新增植物改这里
	switch (plantType) {
	case PlantType::PLANT_SUNFLOWER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<SunFlower>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_SUNFLOWER, row, column,
			AnimationType::ANIM_SUNFLOWER, 1.0f, isPreview);
	case PlantType::PLANT_PEASHOOTER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<PeaShooter>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_PEASHOOTER, row, column,
			AnimationType::ANIM_PEASHOOTER, 1.0f, isPreview);
	case PlantType::PLANT_CHERRYBOMB:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<CherryBomb>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_CHERRYBOMB, row, column,
			AnimationType::ANIM_CHERRYBOMB, 1.0f, isPreview);
	case PlantType::PLANT_WALLNUT:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<WallNut>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_WALLNUT, row, column,
			AnimationType::ANIM_WALLNUT, 1.0f, isPreview);
	case PlantType::PLANT_POTATOMINE:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<PotatoMine>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_POTATOMINE, row, column,
			AnimationType::ANIM_POTATOMINE, 0.8f, isPreview);
	case PlantType::PLANT_SNOWPEA:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<SnowPeaShooter>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_SNOWPEA, row, column,
			AnimationType::ANIM_SNOWPEASHOOTER, 1.0f, isPreview);
	case PlantType::PLANT_CHOMPER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Chomper>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_CHOMPER, row, column,
			AnimationType::ANIM_CHOMPER, 1.0f, isPreview);
	case PlantType::PLANT_REPEATER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Repeater>(
			LAYER_GAME_PLANT, board, PlantType::PLANT_REPEATER, row, column,
			AnimationType::ANIM_REPEAT, 1.0f, isPreview);
	default:
		std::cout << "未知的植物类型: " << static_cast<int>(plantType) << std::endl;
		return nullptr;
	}
}

std::shared_ptr<Zombie> GameAPP::InstantiateZombie(ZombieType zombieType, Board* board, float x, float y, int row, bool isPreview)
{
	// TODO 新增僵尸改这里
	switch (zombieType) {
	case ZombieType::ZOMBIE_NORMAL:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Zombie>(
			LAYER_GAME_ZOMBIE, board, ZombieType::ZOMBIE_NORMAL, x, y, row,
			AnimationType::ANIM_NORMAL_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_TRAFFIC_CONE:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<ConeZombie>(
			LAYER_GAME_ZOMBIE, board, ZombieType::ZOMBIE_TRAFFIC_CONE, x, y, row,
			AnimationType::ANIM_CONE_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_POLEVAULTER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Polevaulter>(
			LAYER_GAME_ZOMBIE, board, ZombieType::ZOMBIE_POLEVAULTER, x, y, row,
			AnimationType::ANIM_POLEVAULTER_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_BUCKET:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<BucketZombie>(
			LAYER_GAME_ZOMBIE, board, ZombieType::ZOMBIE_BUCKET, x, y, row,
			AnimationType::ANIM_BUCKET_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_FASTBUCKET:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<FastBucketZombie>(
			LAYER_GAME_ZOMBIE, board, ZombieType::ZOMBIE_FASTBUCKET, x, y, row,
			AnimationType::ANIM_BUCKET_ZOMBIE, 1.0f, isPreview);
	default:
		std::cout << "[GameAPP::InstantiateZombie] 未知的僵尸类型" << std::endl;
		return nullptr;
	}
}
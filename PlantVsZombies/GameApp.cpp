#include "./GameAPP.h"
#include "./Renderer/VulkanContext.h"
#include "./Renderer/VulkanRenderer.h"
#include "./Renderer/VulkanTexturePool.h"
#include <SDL2/SDL_vulkan.h>
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
#include "./Game/Plant/PuffShroom.h"

#include "./Game/Zombie/Zombie.h"
#include "./Game/Zombie/ConeZombie.h"
#include "./Game/Zombie/Polevaulter.h"
#include "./Game/Zombie/BucketZombie.h"
#include "./Game/Zombie/FastBucketZombie.h"

#include "./Game/Board.h"

#include "./Profiler.h"

#include <iostream>

GameAPP::GameAPP()
	: mInputHandler(nullptr)
	, mWindow(nullptr)
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
	// Phase 3a：Vulkan 窗口。SDL_WINDOW_VULKAN 让 SDL 在创建窗口时加载 Vulkan loader，
	// 并把窗口标记为 Vulkan 兼容（影响 surface 创建路径）。
	mWindow = SDL_CreateWindow(u8"植物大战僵尸中文版",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		SCENE_WIDTH, SCENE_HEIGHT,
		SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

	if (!mWindow) {
		std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
		return false;
	}

	// 校验层：Debug 构建打开；Release 关闭以减少开销。
#ifdef _DEBUG
	const bool enableValidation = true;
#else
	const bool enableValidation = false;
#endif

	m_vulkanCtx = std::make_unique<pvz::VulkanContext>();
	if (!m_vulkanCtx->Initialize(mWindow, enableValidation, mVsync)) {
		std::cerr << "VulkanContext 初始化失败" << std::endl;
		return false;
	}

	m_vulkanRenderer = std::make_unique<pvz::VulkanRenderer>();
	if (!m_vulkanRenderer->Initialize(m_vulkanCtx.get())) {
		std::cerr << "VulkanRenderer 初始化失败" << std::endl;
		return false;
	}

	// Phase 3b：bindless 纹理池——LoadAllResources 把所有纹理上传到这里取得 slot；
	// Graphics::InitializeVulkan 也需要它来构 batch pipeline 的 descriptor layout。
	m_vulkanTexPool = std::make_unique<pvz::VulkanTexturePool>();
	if (!m_vulkanTexPool->Initialize(m_vulkanCtx.get())) {
		std::cerr << "VulkanTexturePool 初始化失败" << std::endl;
		return false;
	}

	// 创建 Graphics 实例并初始化（Phase 3b：CPU 端默认，Vulkan 资源在 InitializeVulkan 时挂上）
	m_graphics = std::make_unique<Graphics>();
	if (!m_graphics->Initialize(SCENE_WIDTH, SCENE_HEIGHT)) {
		std::cerr << "Graphics 初始化失败" << std::endl;
		return false;
	}
	if (!m_graphics->InitializeVulkan(m_vulkanCtx.get(), m_vulkanRenderer.get(), m_vulkanTexPool.get())) {
		std::cerr << "Graphics::InitializeVulkan 失败" << std::endl;
		return false;
	}

	// Task 7: apply A/B toggle from startup flag
	m_graphics->SetInstancePathEnabled(!mDisableInstancePath);
	if (mDisableInstancePath) {
		std::cout << "[Graphics] Instance path 关闭 — reanim 全走 slow path (DrawTextureMatrix)" << std::endl;
	}

	// 设置默认清屏颜色（0~255 输入，内部归一化）。
	// 用黑色：全屏 letterbox 时清屏色只在黑边区域可见，黑边即由此而来；
	// 窗口模式下背景图铺满逻辑画面，清屏色不可见，无副作用。
	m_graphics->SetClearColor(0, 0, 0, 255);

	// 初始化 letterbox 参数（窗口模式 scale=1/offset=0）。
	m_graphics->RecomputeLetterbox();

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

	// Phase 3b：先注入 Vulkan 纹理池，再 Initialize 读 XML；LoadAllResources 上传时就能拿到 pool。
	resourceManager.SetVulkanTexturePool(m_vulkanTexPool.get());

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

	// 玩家存档需要在创建 swapchain 之前加载，否则 mVsync 还是默认值，present mode 选错。
	if (!mGameInfoSaver.LoadPlayerInfo())
	{
		std::cout << "无法加载玩家存档数据！可能是没有存档!" << std::endl;
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
		m_vulkanTexPool.reset();
		m_vulkanRenderer.reset();
		m_vulkanCtx.reset();
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
		m_vulkanTexPool.reset();
		m_vulkanRenderer.reset();
		m_vulkanCtx.reset();
		SDL_DestroyWindow(mWindow);
		TTF_Quit();
		IMG_Quit();
		SDL_Quit();
		return -7;
	}

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

	// 按存档应用全屏设置（mFullscreen 已在 LoadPlayerInfo 时读入）。
	// 放在主循环前、所有渲染资源就绪后执行（帧外，安全）。
	if (mFullscreen) {
		SetFullscreen(true);
	}

	mRunning = true;
	SDL_Event event;

	while (mRunning && !sceneManager.IsEmpty())
	{
		DeltaTime::BeginFrame();
		// 处理事件
		{
			PROFILE_SCOPE("A.InputPoll");
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_QUIT)
				{
					mRunning = false;
				}
				mInputHandler->ProcessEvent(&event);
			}
		}

		// 更新
		{
			PROFILE_SCOPE("B.SceneUpdate_total");
			CursorManager::GetInstance().ResetHoverCount();
			sceneManager.Update();
			CursorManager::GetInstance().Update();
		}

		// 渲染
		{
			PROFILE_SCOPE("C.SceneDraw_total");
			Draw();
		}

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

		Profiler::Get().EndFrame();
	}

	// 清理
	Shutdown();

	return 0;
}

void GameAPP::Draw()
{
	// Phase 3b：Graphics 接管帧生命周期。BeginFrame 负责 acquire+begin+barrier+beginRendering，
	// SceneManager::Draw 累积 batch，EndFrame 把 batch 拷到 GPU、issue draw、submit、present。
	m_graphics->Clear();

	bool ok;
	{
		PROFILE_SCOPE("C1.BeginFrame");
		ok = m_graphics->BeginFrame();
	}
	if (!ok) {
		// acquire 报 OUT_OF_DATE 等：BeginFrame 已置 NeedsSwapchainRebuild，下面统一处理。
	}
	else {
		{
			PROFILE_SCOPE("C2.SceneManagerDraw");
			SceneManager::GetInstance().Draw(m_graphics.get());
		}
		{
			PROFILE_SCOPE("C3.EndFrame_Present");
			m_graphics->EndFrame();
		}
	}

	// 帧外消化 swapchain rebuild 请求（OUT_OF_DATE / SUBOPTIMAL）。vsync 主动切换走 ApplyVsync 直接重建，
	// 这里只兜底未来的窗口大小变化、Alt+Tab 全屏切换等情况。
	// 注意：RecreateSwapchain 在窗口最小化/隐藏（extent=0x0）时返回 false 且不销毁旧 swapchain，
	// 此时跳过 OnSwapchainRecreated，保留 rebuild 标志，下一帧继续重试。
	if (m_vulkanRenderer && m_vulkanRenderer->NeedsSwapchainRebuild()) {
		if (m_vulkanCtx->RecreateSwapchain(mVsync)) {
			m_vulkanRenderer->OnSwapchainRecreated();
			m_vulkanRenderer->ClearSwapchainRebuildFlag();
			// 交换链尺寸可能变了（窗口拉伸 / Alt+Tab 全屏切换的兜底路径），重算 letterbox。
			if (m_graphics) m_graphics->RecomputeLetterbox();
		}
	}
}

bool GameAPP::ApplyVsync(bool vsync)
{
	if (!m_vulkanCtx || !m_vulkanRenderer) return false;
	mVsync = vsync;
	if (!m_vulkanCtx->RecreateSwapchain(mVsync)) return false;
	if (!m_vulkanRenderer->OnSwapchainRecreated()) return false;
	m_vulkanRenderer->ClearSwapchainRebuildFlag();
	if (m_graphics) m_graphics->RecomputeLetterbox();
	return true;
}

bool GameAPP::SetFullscreen(bool fullscreen)
{
	if (!mWindow || !m_vulkanCtx || !m_vulkanRenderer || !m_graphics) return false;

	// FULLSCREEN_DESKTOP：沿用桌面分辨率、不切显示模式、Alt-Tab 顺滑。0 = 还原窗口。
	Uint32 flag = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
	if (SDL_SetWindowFullscreen(mWindow, flag) != 0) {
		std::cerr << "SDL_SetWindowFullscreen 失败: " << SDL_GetError() << std::endl;
		return false;
	}
	mFullscreen = fullscreen;

	// 交换链尺寸随之改变，需热重建并重算 letterbox（缩放比 + 黑边偏移）。
	if (!m_vulkanCtx->RecreateSwapchain(mVsync)) return false;
	if (!m_vulkanRenderer->OnSwapchainRecreated()) return false;
	m_vulkanRenderer->ClearSwapchainRebuildFlag();
	m_graphics->RecomputeLetterbox();
	return true;
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

	// 清理资源管理器 (会通过 VulkanTexturePool 释放每张 bindless 纹理 —— 此时 pool 必须仍存活)
	ResourceManager::ReleaseInstance();

	// 清理文字缓存 (Graphics 内部有缓存)
	if (m_graphics) {
		m_graphics->ClearTextCache();
	}

	// 清理音频系统
	AudioSystem::Shutdown();

	// 清理输入处理器
	mInputHandler.reset();

	// 清理 Graphics（Phase 3b：内部会先 ShutdownVulkan 释放 pipeline / 缓冲 / descriptor pool —— 需要 ctx 仍存活）
	m_graphics.reset();

	// 清理 Vulkan：先纹理池（依赖 ctx），再 renderer，最后 ctx；都在 SDL_DestroyWindow 之前——
	// VulkanContext 析构会销毁 surface，而 surface 来自该窗口
	m_vulkanTexPool.reset();
	m_vulkanRenderer.reset();
	m_vulkanCtx.reset();

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
	// TODO: 新增地图改这里
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

bool GameAPP::GetBackgroundIsNight(Background background) const
{
	if (background == Background::GROUND_NIGHT || background == Background::NIGHT_WATER_POOL
		|| background == Background::NIGHT_ROOF)
	{
		return true;
	}
	else
	{
		return false;
	}
}

std::shared_ptr<Plant> GameAPP::InstantiatePlant(PlantType plantType, Board* board, int row, int column, bool isPreview)
{
	// TODO 新增植物改这里
	switch (plantType) {
	case PlantType::PLANT_SUNFLOWER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<SunFlower>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_SUNFLOWER, 1.0f, isPreview);
	case PlantType::PLANT_PEASHOOTER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<PeaShooter>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_PEASHOOTER, 1.0f, isPreview);
	case PlantType::PLANT_CHERRYBOMB:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<CherryBomb>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_CHERRYBOMB, 1.0f, isPreview);
	case PlantType::PLANT_WALLNUT:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<WallNut>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_WALLNUT, 1.0f, isPreview);
	case PlantType::PLANT_POTATOMINE:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<PotatoMine>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_POTATOMINE, 0.8f, isPreview);
	case PlantType::PLANT_SNOWPEA:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<SnowPeaShooter>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_SNOWPEASHOOTER, 1.0f, isPreview);
	case PlantType::PLANT_CHOMPER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Chomper>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_CHOMPER, 1.0f, isPreview);
	case PlantType::PLANT_REPEATER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Repeater>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_REPEAT, 1.0f, isPreview);
	case PlantType::PLANT_PUFFSHROOM:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<PuffShroom>(
			LAYER_GAME_PLANT, board, plantType, row, column,
			AnimationType::ANIM_PUFFSHROOM, 1.0f, isPreview);
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
			LAYER_GAME_ZOMBIE, board, zombieType, x, y, row,
			AnimationType::ANIM_NORMAL_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_TRAFFIC_CONE:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<ConeZombie>(
			LAYER_GAME_ZOMBIE, board, zombieType, x, y, row,
			AnimationType::ANIM_CONE_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_POLEVAULTER:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Polevaulter>(
			LAYER_GAME_ZOMBIE, board, zombieType, x, y, row,
			AnimationType::ANIM_POLEVAULTER_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_BUCKET:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<BucketZombie>(
			LAYER_GAME_ZOMBIE, board, zombieType, x, y, row,
			AnimationType::ANIM_BUCKET_ZOMBIE, 1.0f, isPreview);
	case ZombieType::ZOMBIE_FASTBUCKET:
		return GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<FastBucketZombie>(
			LAYER_GAME_ZOMBIE, board, zombieType, x, y, row,
			AnimationType::ANIM_BUCKET_ZOMBIE, 1.0f, isPreview);
	default:
		std::cout << "[GameAPP::InstantiateZombie] 未知的僵尸类型" << std::endl;
		return nullptr;
	}
}
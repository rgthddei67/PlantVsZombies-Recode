#include "./GameAPP.h"
#include "./UI/InputHandler.h"
#include "./ResourceManager.h"
#include "./Game/Board.h"
#include "./Game/SceneManager.h"
#include "./Game/GameScene.h"
#include "./CursorManager.h"
#include "./Game/AudioSystem.h"
#include "./GameRandom.h"
#include "./DeltaTime.h"
#include "./ParticleSystem/ParticleSystem.h"
#include "./Game/GameObjectManager.h"
#include "./Game/CollisionSystem.h"
#include "./Game/Plant/GameDataManager.h"
#include <iostream>
#include <sstream>

GameAPP::GameAPP()
    : mInputHandler(nullptr)
    , mWindow(nullptr)
    , m_glContext(nullptr)
    , mRunning(false)
    , mInitialized(false)
{
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

    if (SDL_GL_SetSwapInterval(1) != 0) {
        std::cerr << "警告：无法启用垂直同步，错误：" << SDL_GetError() << std::endl;
    }

    // 创建 Graphics 实例并初始化
    m_graphics = std::make_unique<Graphics>();
    if (!m_graphics->Initialize(SCENE_WIDTH, SCENE_HEIGHT)) {
        std::cerr << "Graphics 初始化失败" << std::endl;
        return false;
    }

    // 设置默认清屏颜色
    m_graphics->SetClearColor(0, 0, 0, 1);

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

    mInputHandler = std::make_unique<InputHandler>(m_graphics.get());
    g_particleSystem = std::make_unique<ParticleSystem>(m_graphics.get()); 

    auto& sceneManager = SceneManager::GetInstance();
    sceneManager.RegisterScene<GameScene>("GameScene");
    sceneManager.SwitchTo("GameScene");

    DeltaTime::Reset();

    mRunning = true;
    SDL_Event event;

    while (mRunning && !sceneManager.IsEmpty())
    {
        Uint64 start = SDL_GetPerformanceCounter();

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

        // 特殊按键处理
        if (mInputHandler->IsKeyReleased(SDLK_F3))
        {
            AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BUTTONCLICK, 0.5f);
            if (!mDebugMode)
                DeltaTime::SetTimeScale(10.0f);
            else
                DeltaTime::SetTimeScale(1.0f);
            mDebugMode = !mDebugMode;
            mShowColliders = !mShowColliders;
        }

        if (mInputHandler->IsKeyReleased(SDLK_ESCAPE))
        {
            mRunning = false;
            break;
        }

        // 更新
        CursorManager::GetInstance().ResetHoverCount();
        sceneManager.Update();
        CursorManager::GetInstance().Update();

        // 渲染
        Draw(start);

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
    }

    // 清理
    Shutdown();

    return 0;
}

void GameAPP::Draw(Uint64 start)
{
    // 清除颜色缓冲
    m_graphics->Clear();

    // 处理多线程渲染命令 
    m_graphics->ProcessCommandQueue();

    // 绘制场景 (传入 Graphics 对象)
    SceneManager::GetInstance().Draw(m_graphics.get());

    // 提交批处理并执行绘制
    m_graphics->FlushBatch();

    // 在 SwapWindow 前主动 sleep，避免驱动忙等待吃满 CPU
    Uint64 now = SDL_GetPerformanceCounter();
    float elapsedMS = (now - start) * 1000.0f / SDL_GetPerformanceFrequency();
    const float targetMS = 1000.0f / 60.0f;
    if (elapsedMS < targetMS - 2.0f) {
        SDL_Delay((Uint32)(targetMS - elapsedMS - 2.0f));
    }

    // 交换缓冲区
    SDL_GL_SwapWindow(mWindow);
}

void GameAPP::Shutdown()
{
    if (mRunning) return;

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
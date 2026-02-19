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
    , mRenderer(nullptr)
    , mRunning(false)
    , mInitialized(false)
{
	mTextCache.reserve(16);
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
    // 设置SDL提示
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");          // 启用垂直同步减少撕裂

    // 创建窗口
    mWindow = SDL_CreateWindow(u8"植物大战僵尸中文版",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCENE_WIDTH, SCENE_HEIGHT,
        SDL_WINDOW_SHOWN);

    if (!mWindow) {
        std::cerr << "窗口创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    // 创建渲染器
    mRenderer = SDL_CreateRenderer(mWindow, -1,
        SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC |
        SDL_RENDERER_TARGETTEXTURE);

    if (!mRenderer)
    {
        std::cerr << "渲染器创建失败: " << SDL_GetError() << std::endl;
        return false;
    }

    // 设置初始背景色
    SDL_SetRenderDrawColor(mRenderer, 0, 0, 255, 255);
    SDL_RenderClear(mRenderer);
    SDL_RenderPresent(mRenderer);

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

    if (!resourceManager.Initialize(mRenderer, "./resources/resources.xml")) {
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

    // 初始化输入处理器
    mInputHandler = std::make_unique<InputHandler>();

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
        
    }

    // 初始化GameAPP自身
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
        SDL_DestroyRenderer(mRenderer);
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
        SDL_DestroyRenderer(mRenderer);
        SDL_DestroyWindow(mWindow);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return -7;
    }

    g_particleSystem = std::make_unique<ParticleSystem>(mRenderer);

    auto& sceneManager = SceneManager::GetInstance();
    sceneManager.RegisterScene<GameScene>("GameScene");
    sceneManager.SwitchTo("GameScene");

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

        // 特殊按键处理
        if (mInputHandler->IsKeyReleased(SDLK_F3))
        {
            AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BUTTONCLICK, 0.5f);
			if (!mDebugMode)
			    DeltaTime::SetTimeScale(5.0f);
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
        Draw();

#ifdef _DEBUG
        static int MousePoint = 0;
        if (MousePoint++ % 40 == 0)
        {
            std::cout << "Mouse Position: "
                << mInputHandler->GetMousePosition().x << ", "
                << mInputHandler->GetMousePosition().y << std::endl;
        }
#endif

        mInputHandler->Update();
    }

    // 清理
    Shutdown();

    return 0;
}

void GameAPP::Draw()
{
    // 清屏
    SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
    SDL_RenderClear(mRenderer);

    // 绘制场景
    auto& sceneManager = SceneManager::GetInstance();
    sceneManager.Draw(mRenderer);

    // 更新屏幕
    SDL_RenderPresent(mRenderer);
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

    // 清理资源管理器
    ResourceManager::ReleaseInstance();

    // 清理文本缓存
    CleanupResources();

    // 清理音频系统
    AudioSystem::Shutdown();

    // 清理渲染器
    SDL_DestroyRenderer(this->mRenderer);
    this->mRenderer = nullptr;

    // 清理输入处理器
    if (mInputHandler) {
        mInputHandler.reset();
    }

    // 清理窗口
    if (mWindow) {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }

    // 清理光标管理器
    CursorManager::GetInstance().Cleanup();

    // 清理SDL子系统
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    mRunning = false;
    mInitialized = false;
}

void GameAPP::ClearTextCache()
{
    for (size_t i = 0; i < mTextCache.size(); i++)
    {
        if (!mTextCache[i].texture) continue;
        SDL_DestroyTexture(mTextCache[i].texture);
	}

    mTextCache.clear();
    std::cout << "清除文本缓存" << std::endl;
}

void GameAPP::CleanupResources()
{
    ClearTextCache();
    ResourceManager::GetInstance().CleanupUnusedFontSizes();
}

SDL_Texture* GameAPP::GetCachedTextTexture(const std::string& text,
    const SDL_Color& color,
    const std::string& fontKey,
    int fontSize,
    int& outWidth,
    int& outHeight)
{
    // 1. 生成唯一缓存键
    std::stringstream ss;
    ss << text << "|" << fontKey << "|" << fontSize << "|"
        << (int)color.r << "," << (int)color.g << "," << (int)color.b << "," << (int)color.a;
    std::string key = ss.str();

    // 2. 查找缓存
    for (size_t i = 0; i < mTextCache.size(); i++)
    {
        if (mTextCache[i].key == key) {
            outWidth = mTextCache[i].width;
            outHeight = mTextCache[i].height;
            return mTextCache[i].texture;
		}
    }

    // 3. 缓存未命中，创建新纹理
    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font) {
        outWidth = outHeight = 0;
        return nullptr;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        outWidth = outHeight = 0;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(mRenderer, surface);
    outWidth = surface->w;
    outHeight = surface->h;
    SDL_FreeSurface(surface);

    if (!texture) {
        return nullptr;
    }

    // 4. 存入缓存
    mTextCache.push_back({ key, texture, outWidth, outHeight });

    return texture;
}

void GameAPP::DrawText(const std::string& text,
    int x, int y,
    const SDL_Color& color,
    const std::string& fontKey,
    int fontSize)
{
    if (text.empty()) return;

    int w, h;
    SDL_Texture* texture = GetCachedTextTexture(text, color, fontKey, fontSize, w, h);
    if (!texture) return;

    SDL_Rect dest = { x, y, w, h };
    SDL_RenderCopy(mRenderer, texture, nullptr, &dest);
}

void GameAPP::DrawText(const std::string& text,
    const Vector& position,
    const SDL_Color& color,
    const std::string& fontKey,
    int fontSize)
{
    DrawText(text, static_cast<int>(position.x), static_cast<int>(position.y),
        color, fontKey, fontSize);
}

Vector GameAPP::GetTextSize(const std::string& text,
    const std::string& fontKey,
    int fontSize)
{
    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font)
    {
        return Vector::zero();
    }

    int width = 0, height = 0;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    return Vector(static_cast<float>(width), static_cast<float>(height));
}

SDL_Texture* GameAPP::CreateTextTexture(SDL_Renderer* renderer,
    const std::string& text,
    const SDL_Color& color,
    const std::string& fontKey,
    int fontSize)
{
    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font) return nullptr;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;  // 调用者负责销毁
}
#pragma once
#ifndef _GAMEAPP_H
#define _GAMEAPP_H
#ifdef DrawText
#undef DrawText
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <stdexcept>
#include "ResourceKeys.h"
#include "./Game/Definit.h"
#include "./ParticleSystem/ParticleSystem.h"
#include "GameInfoSaver.h"
#include "./Game/Plant/PlantType.h"
#include "./Game/Zombie/ZombieType.h"
#include "Graphics.h"

constexpr int SCENE_WIDTH = 1100;
constexpr int SCENE_HEIGHT = 600;

class InputHandler;
class Board;
class Plant;
class Zombie;

namespace pvz {
	class VulkanContext;
	class VulkanRenderer;
	class VulkanTexturePool;
}

enum class Background;

class GameAPP
{
public:
	int Difficulty = 1; // 难度系数
	int mAdventureLevel = 1;    // 玩到的冒险模式关卡
	bool mShowPlantHP = false;  // 植物显示血量
	bool mShowZombieHP = false; // 僵尸显示血量
	bool mAutoCollected = true; // 自动收集
	bool mVsync = false;    // 是否开启垂直同步

	std::vector<PlantType> mHaveCards;      // 玩家拥有的卡牌

	GameInfoSaver mGameInfoSaver;

private:
	std::unique_ptr<InputHandler> mInputHandler;
	std::unique_ptr<Graphics> m_graphics;   // 改用 Graphics

	SDL_Window* mWindow;
	// Phase 3a/3b：Vulkan 接管。VulkanContext / VulkanRenderer 取代了 SDL_GLContext。
	// Phase 3b 起增加 VulkanTexturePool，承载 bindless 纹理槽位。
	std::unique_ptr<pvz::VulkanContext>     m_vulkanCtx;
	std::unique_ptr<pvz::VulkanRenderer>    m_vulkanRenderer;
	std::unique_ptr<pvz::VulkanTexturePool> m_vulkanTexPool;

	bool mRunning;
	bool mInitialized;

	GameAPP();
	~GameAPP();

	GameAPP(const GameAPP&) = delete;
	GameAPP& operator=(const GameAPP&) = delete;

	bool InitializeSDL();
	bool InitializeSDL_Image();
	bool InitializeSDL_TTF();
	bool InitializeAudioSystem();
	bool CreateWindowAndRenderer();
	bool InitializeResourceManager();
	bool LoadAllResources();
	void CleanupResources();
	void Draw();
	void Shutdown();

public:
	inline static bool mDebugMode = false;        // 是否是调试模式
	inline static bool mShowColliders = false;    // 显示碰撞框开关
	inline static bool mDisableInstancePath = false;  // Task 7: -NoInstance 启动参数禁用 GPU instance path

	static GameAPP& GetInstance();

	int Run();
	bool Initialize();

	// 获取 Graphics 对象
	Graphics& GetGraphics() { return *m_graphics; }

	// 应用新的垂直同步设置：写 mVsync + 热重建 swapchain（不重启）。
	// 必须在主线程、帧外（不在 BeginFrame..EndFrame 之间）调用。
	bool ApplyVsync(bool vsync);

	// 设置游戏是否运行
	void SetRunning(bool running) { this->mRunning = running; }

	// 世界坐标绘制文本 UTF8编码
	void DrawText(const std::string& text,
		const Vector& position,
		const glm::vec4& color,
		const std::string& fontKey = ResourceKeys::Fonts::FONT_FZCQ,
		int fontSize = 17);

	// 获取当前背景索引(根据关卡)
	Background GetBackgroundID(int level) const;

	// 获取输入处理器
	InputHandler& GetInputHandler() const {
		if (!mInputHandler) {
			throw std::runtime_error("InputHandler not initialized");
		}
		return *mInputHandler;
	}

	bool IsInputHandlerValid() const { return mInputHandler != nullptr; }

	// 获取窗口 (可能用于其他目的)
	SDL_Window* GetWindow() const { return mWindow; }

	std::shared_ptr<Plant> InstantiatePlant(PlantType plantType, Board* board, int row, int column, bool isPreview = false);
	std::shared_ptr<Zombie> InstantiateZombie(ZombieType zombieType, Board* board, float x, float y, int row, bool isPreview = false);
};

#endif
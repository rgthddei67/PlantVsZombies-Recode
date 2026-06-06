#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./GameAPP.h"
#include "GameMonitor.h"
#include "Logger.h"
#include <SDL2/SDL.h>
#include <string>
#include <iostream>

int main(int argc, char** argv)
{
    // TEMP smoke test —— 验证后删除
    LOG_TRACE("Smoke") << "trace level " << 1;
    LOG_DEBUG("Smoke") << "debug level " << 2;
    LOG_INFO ("Smoke") << "info level "  << 3;
    LOG_WARN ("Smoke") << "warn level "  << 4;
    LOG_ERROR("Smoke") << "error level " << 5;

	system("chcp 65001");
	system("cls");
	CrashHandler::Initialize();
	GameRandom::RandomizeSeed();

	GameMonitor::Init();

	// 检查命令行参数
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "-Debug" || arg == "-debug")
		{
			GameAPP::mDebugMode = true;
			GameAPP::mShowColliders = true;
			std::cout << "Debug模式已启用" << std::endl;
		}
		else if (arg == "-NoInstance" || arg == "-noinstance")
		{
			GameAPP::mDisableInstancePath = true;
			std::cout << "GPU instance path 已禁用 (A/B baseline)" << std::endl;
		}
	}

	int result = GameAPP::GetInstance().Run();

	CrashHandler::Cleanup();

	return result;
}